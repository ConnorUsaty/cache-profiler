#include <new>
#include <chrono>
#include <vector>
#include <iostream>
#include <random>

std::vector<size_t> gen_rand_idx(size_t cache_sz){
    std::vector<size_t> idx{};
    idx.reserve(cache_sz/64U);
    for(size_t i=0; i<cache_sz; i+=64U){
        idx.emplace_back(i);
    }
    
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 generator(seed);
    std::shuffle(idx.begin(), idx.end(), generator);

    return idx;
}


int main(){
    size_t constexpr CACHELINE_SIZE = std::hardware_destructive_interference_size;
    size_t constexpr BUF_SZ = (1U << 18U);
    size_t constexpr n_iterations = 10000U;

    uint8_t* buffer = reinterpret_cast<uint8_t*>(new uint8_t[BUF_SZ]);

    std::vector<std::pair<size_t,long long>> measurements; // {cache_sz, access time (ns)}

    for(size_t curr_sz=64U; curr_sz<=BUF_SZ; curr_sz <<= 1U){
        auto rand_idxs = gen_rand_idx(curr_sz);
        
        // warm cache
        for(size_t i=0; i<curr_sz; i += CACHELINE_SIZE){
            buffer[i] ^= 0x0101;
            volatile uint8_t v = buffer[i];
            (void)v;
        }

        // time memory accesses
        auto start = std::chrono::high_resolution_clock::now();
        for(size_t i=0; i<curr_sz/64U; ++i){
            size_t idx = rand_idxs[i];
            volatile uint8_t v = buffer[idx];
            (void)v;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / (long long)rand_idxs.size();
        measurements.push_back({curr_sz, elapsed});
    }

    // dump results to log file
    for(auto& [sz, time] : measurements){
        std::cout << "SIZE: " << sz << " ACCESS TIME: " << time << "\n";
    }

    return 0;
}