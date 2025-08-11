#include <chrono>
#include <vector>
#include <iostream>
#include <random>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <numeric>
#include <memory>
#include <filesystem>

// TODO: use std::hardware_destructive_interference_size (i.e. do not assume cache line size)
#define CACHELINE_SIZE 64


// TODO: clearly this only works on linux, make another one that works on windows
#ifdef __linux__
#include <sched.h>
#include <pthread.h>

// ensuring this program continues to run on the same core is very important
// if we keep getting scheduled on different cores we lose all the state
// we built up in the previous cores cache, thus likely ruining all L1 and L2 cache measurements
bool pin_to_core(int core_id) {
    // set cpuset mask to only core_id
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    
    // pin this thread to only core_id
    pthread_t thread = pthread_self();
    int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        std::cerr << "Failed to set CPU affinity: " << strerror(result) << "\n";
        return false;
    }
    
    // verify the affinity was set
    CPU_ZERO(&cpuset);
    if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0) {
        std::cout << "Thread pinned to core " << core_id << "\n";
        return true;
    }
    return false;
}
#endif

struct CacheTestResult {
    size_t size_kb;
    double latency_ns;
    double throughput_mbps;
};

// pointer chasing prevents hardware prefetching
// hardware prefetching ruins our measurements even with proper cache set up and access patterns
struct alignas(CACHELINE_SIZE) CacheLine {
    CacheLine* next;
    char padding[CACHELINE_SIZE - sizeof(CacheLine*)]; // this makes the struct exactly one cache line
};

// TODO: fix so that I can use the attribute
// the line below is causing my intellisense to break for some reason so commented out for now
// __attribute__((always_inline)) 
inline void prevent_compiler_optimization(CacheLine* ptr) {
    // inline assembly to prevent compiler optimizations
    // TODO: make more robust for non x86 systems
    #ifdef __x86_64__
        __asm__ volatile("" : : "r" (ptr) : "memory");
    #endif
}

CacheLine* create_pointer_chain(size_t size_bytes) {
    size_t n_elements = size_bytes / sizeof(CacheLine);
    if (n_elements == 0) n_elements = 1;
    
    // aligned_alloc is only on Linux
    // TODO: use #ifdef guards to make this work on all systems
    CacheLine* buffer = static_cast<CacheLine*>(aligned_alloc(64, n_elements * sizeof(CacheLine)));
    if (!buffer) {
        throw std::bad_alloc();
    }
    
    // shuffle to create random access pattern
    std::vector<size_t> indices(n_elements);
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);
    
    // link the cacheline nodes
    for (size_t i = 0; i < n_elements - 1; ++i) {
        buffer[indices[i]].next = &buffer[indices[i + 1]];
    }
    buffer[indices[n_elements - 1]].next = &buffer[indices[0]]; // make it circular
    
    return buffer;
}

// measure cache latency using pointer chasing
double measure_latency(CacheLine* start, size_t iterations) {    
    auto start_time = std::chrono::high_resolution_clock::now();

    CacheLine* ptr = start;
    for (size_t i = 0; i < iterations; ++i) {
        ptr = ptr->next;
        prevent_compiler_optimization(ptr);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    
    // use ptr to prevent compiler optimizations
    volatile CacheLine* dummy = ptr;
    (void)dummy;
    
    return static_cast<double>(duration) / iterations;
}

void flush_cache() {
    const size_t FLUSH_SIZE = 32 * 1024 * 1024; // 32MB to flush most L3 caches
    char* flush_buffer = new char[FLUSH_SIZE];
    
    // write to entire buffer to flush caches
    std::memset(flush_buffer, 0, FLUSH_SIZE);
    
    // force compiler to not optimize this away
    volatile char dummy = flush_buffer[0];
    (void)dummy;
    
    delete[] flush_buffer;
}

void warm_cache(CacheLine* start, size_t n_cachelines) {
    // warm cache (cannot measure cache latency if our data is still in RAM)
    CacheLine* ptr = start;
    for (size_t i = 0; i < n_cachelines; ++i) {
        ptr = ptr->next;
        prevent_compiler_optimization(ptr);
    }
    
    // use ptr to prevent compiler optimizations
    volatile CacheLine* dummy = ptr;
    (void)dummy;
}

std::vector<CacheTestResult> run_cache_tests() {
    std::vector<CacheTestResult> results;
    std::vector<size_t> test_sizes_kb = {
        4, 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768,
        1024, 1536, 2048, 3072, 4096, 6144, 8192
    };
    
    const size_t ITERATIONS = 10000000;
    const size_t SAMPLES = 10; // num of samples per test size
    
    std::cout << std::setw(12) << "Size (KB)" 
              << std::setw(15) << "Latency (ns)" 
              << std::setw(20) << "Throughput (MB/s)" 
              << "\n";
    std::cout << std::string(47, '-') << "\n";
    
    for (size_t size_kb : test_sizes_kb) {
        size_t size_bytes = size_kb * 1024;
        size_t n_cachelines = size_bytes / CACHELINE_SIZE;
        std::vector<double> latencies;
        
        for (size_t sample = 0; sample < SAMPLES; ++sample) {
            CacheLine* chain = create_pointer_chain(size_bytes);

            // flush any data in caches from previous test
            flush_cache();
            // warm cache with the data for this test
            warm_cache(chain, n_cachelines);
            
            double latency = measure_latency(chain, ITERATIONS);
            latencies.push_back(latency);
            free(chain);
        }

        // calculate and store results
        std::sort(latencies.begin(), latencies.end());
        double median_latency = latencies[SAMPLES / 2];
        double throughput_mbps = (64.0 / median_latency) * 1000.0; // 64 bytes per access, convert ns to MB/s
        CacheTestResult result{size_kb, median_latency, throughput_mbps};
        results.push_back(result);
        
        // output result to terminal for user to see as programs running
        std::cout << std::setw(12) << size_kb 
                  << std::setw(15) << std::fixed << std::setprecision(2) << median_latency
                  << std::setw(20) << std::fixed << std::setprecision(2) << throughput_mbps
                  << "\n";
    }
    
    return results;
}

void ensure_directory_exists(const std::string& dir_path) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(dir_path)) {
        if (fs::create_directories(dir_path)) {
            std::cout << "Created directory: " << dir_path << "\n";
        } else {
            throw std::runtime_error("Failed to create directory: " + dir_path);
        }
    }
}

std::string get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%d_%m_%Y-%H_%M_%S");
    return ss.str();
}

void generate_output_csv(const std::vector<CacheTestResult>& results, const std::string& dir_path) {
    std::string timestamp = get_timestamp_string();
    std::string filename = dir_path + "results_" + timestamp + ".csv";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << filename << "\n";
        return;
    }
    
    file << "Size_KB,Latency_ns,Throughput_MBps\n";
    for (const auto& result : results) {
        file << result.size_kb << "," 
             << result.latency_ns << "," 
             << result.throughput_mbps << "\n";
    }
    
    file.close();
    std::cout << "\nResults saved to " << filename << "\n";
}

int main() {
    std::cout << "\ncache_profiler.cpp\n";
    std::cout << "========================================\n";
    
    try {
        const std::string dir_path = "../measurements/";
        ensure_directory_exists(dir_path);

        // TODO: allow pinning on all systems
        #ifdef __linux__
            int target_core = 1;
            if (!pin_to_core(target_core)) {
                std::cerr << "Warning: Could not pin to core " << target_core 
                        << ", measurements may be less consistent\n";
            }
        #endif
        
        std::cout << "Running tests...\n\n";
        auto results = run_cache_tests();
        generate_output_csv(results, dir_path);
    } 
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}