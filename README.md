# Cache Profiler

A high-precision CPU cache profiling tool that estimates the size and memory access latency across the L1, L2, and L3 caches.

## Overview

This project uses a pointer-chasing technique to measure cache latency while preventing hardware prefetching from skewing results. By testing progressively larger data sets, it reveals the performance characteristics of your CPU's cache hierarchy. The tool outputs both raw CSV data and generates visualizations to clearly show cache level transitions. 

NOTE: A drawback to the current implementation is that access times appear to scale linearly within the same cache. Obviously all of L2 cache should have approximately the same memory access latency, but with the current implementation as the test size increases while staying within L2 bounds, we get more hits in L2 proportional to our L1 hits thus increasing the median latency while still being inside the L2 cache bounds.

## Features

- **Precise Latency Measurement**: Uses pointer chasing with cache-line aligned structures to prevent hardware prefetching
- **CPU Core Pinning**: Pins execution to a specific core (Linux) to maintain cache state consistency
- **Multiple Sample Collection**: Takes multiple measurements per test size and reports median values
- **Cache Warming**: Properly warms caches before measurement to ensure accurate results
- **Cache Flushing**: Clears caches between tests to prevent interference
- **Automated Visualization**: Generates timestamped graphs showing latency vs. data set size
## Requirements

### Current System Requirements
- Linux
- Modern C++ compiler with C++17 support
- CMake
- Python 3.x

### Dependencies
- **C++**: Standard library, pthread (Linux)
- **Python**: matplotlib
- **Build**: cmake, make

## Usage

### Quick Start
```bash
# Clone and navigate to project directory
git clone <your-repo>
cd cache-profiler

# Run the complete profiling and visualization pipeline
./profile_and_vis.sh
```

### Manual Usage

#### Build the profiler:
```bash
mkdir -p build
cd build
cmake ..
make
```

#### Run cache profiling:
```bash
./cache_profiler
```

#### Visualize results:
```bash
# Setup Python environment (first time only)
python3 -m venv venv
source venv/bin/activate
pip install uv
uv pip install -r requirements.txt

# Generate visualization
python3 visualize_measurements.py
```

## Output

The tool generates two types of output:

1. **CSV Data**: Timestamped files in `measurements/` containing:
   - Test size (KB)
   - Median latency (ns)
   - Throughput (MB/s)

2. **Visualizations**: Timestamped graphs in `graphs/` showing latency curves that reveal cache boundaries

### Example Output Graph

<img width="662" height="485" alt="image" src="https://github.com/user-attachments/assets/a65cfd4d-70b4-4237-ad38-d896373a8a13" />


## Understanding Results

The latency graph typically shows distinct plateaus and jumps:
- **Low latency plateau**: Data fits in L1 cache (~32-64KB)
- **First jump**: L1 cache exceeded, now accessing L2 cache
- **Second jump**: L2 cache exceeded, now accessing L3 cache  
- **Major spike**: L3 cache exceeded, accessing main memory

## TODOs
- Implement Windows support for core pinning
- Add macOS support for core pinning
- Replace `aligned_alloc` with cross-platform memory allocation
- Add non-x86 architecture support for compiler optimization prevention
- Use `std::hardware_destructive_interference_size` instead of hardcoded cache line size
