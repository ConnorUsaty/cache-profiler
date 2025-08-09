#include <new>
#include <chrono>
#include <vector>
#include <iostream>
#include <random>

std::vector<size_t> gen_rand_idx(size_t n_cachelines){
    std::vector<size_t> idx{};
    idx.reserve(n_cachelines);
    for(size_t i=0; i<n_cachelines; ++i){
        idx.emplace_back(i);
    }
    
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 generator(seed);
    std::shuffle(idx.begin(), idx.end(), generator);

    return idx;
}


int main(){
    size_t constexpr CACHELINE_SIZE = std::hardware_destructive_interference_size;
    size_t constexpr BUF_SZ = (1U << 24U);
    size_t constexpr N_CACHELINES = BUF_SZ / CACHELINE_SIZE;
    size_t constexpr N_ITERS = 5U;
    
    
    std::vector<std::pair<size_t,long long>> measurements; // {cache_sz, access time (ns)}
    
    for(size_t i=0U; i<N_ITERS; ++i){
        uint8_t* buffer = reinterpret_cast<uint8_t*>(new uint8_t[BUF_SZ]);

        // group in batches for more accurate results
        size_t batch_sz = 200U;
        size_t n_batches = N_CACHELINES / batch_sz;
        auto rand_indexes = gen_rand_idx(batch_sz);
        
        // warm cache to get desired memory layout
        // memory layout should be like this with lower mem address on left:
        // RAM ... | L3 ... | L2 ... | L1 ...
        for(size_t i=0; i<BUF_SZ; i+=CACHELINE_SIZE) {
            volatile uint8_t v = buffer[i];
            (void)v;
        }

        // start from right (L1)
        size_t buf_idx = BUF_SZ-1;
        
        // time memory accesses
        for(size_t batch=0; batch < n_batches; ++batch){

            auto start = std::chrono::high_resolution_clock::now();
            for(size_t i=0; i < batch_sz; ++i){
                size_t idx = rand_indexes[i];
                volatile uint8_t v = buffer[buf_idx - (idx * CACHELINE_SIZE)];
                (void)v;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / batch_sz;

            buf_idx -= (batch_sz * CACHELINE_SIZE);
            measurements.push_back({buf_idx, elapsed});
        }

        delete[] buffer;
    }
    

    // dump results to log file
    for(auto& [sz, time] : measurements){
        std::cout << "SIZE: " << sz << " ACCESS TIME: " << time << "\n";
    }

    return 0;
}