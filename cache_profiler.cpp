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
    size_t constexpr BUF_SZ = (1U << 31U);
    
    std::vector<std::pair<size_t,long long>> measurements; // {cache_sz, access time (ns)}
    uint8_t* buffer = reinterpret_cast<uint8_t*>(new uint8_t[BUF_SZ]);

    for(size_t curr_sz=64U; curr_sz<=BUF_SZ; curr_sz <<= 1U){
        size_t n_cachelines = curr_sz / CACHELINE_SIZE;
        
        // warm cache to get desired memory layout
        // memory layout should be like this with lower mem address on left:
        // RAM ... | L3 ... | L2 ... | L1 ...
        for(size_t i=0; i<curr_sz; i += CACHELINE_SIZE){
            buffer[i] ^= 0x0101;
            volatile uint8_t v = buffer[i];
            (void)v;
        }

        // group in batches for more accurate results
        size_t batch_sz = 1000U;
        size_t n_batches = n_cachelines / batch_sz;
        
        // start from right (L1)
        size_t buf_idx = curr_sz-1;
        
        // time memory accesses
        for(size_t batch=0; batch < n_batches; ++batch){
            auto rand_indexes = gen_rand_idx(batch_sz);

            auto start = std::chrono::high_resolution_clock::now();
            for(size_t i=0; i < batch_sz; ++i){
                size_t idx = rand_indexes[i];
                volatile uint8_t v = buffer[buf_idx - (idx * CACHELINE_SIZE)];
                (void)v;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

            buf_idx -= (batch_sz * CACHELINE_SIZE);
            measurements.push_back({buf_idx * CACHELINE_SIZE, elapsed});
        }
    }
    
    delete[] buffer;

    // dump results to log file
    for(auto& [sz, time] : measurements){
        std::cout << "SIZE: " << sz << " ACCESS TIME: " << time << "\n";
    }

    return 0;
}