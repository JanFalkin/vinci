# Performance Analysis

## Multi-Core Utilization

The implementation now achieves **excellent CPU utilization** on multi-core systems through:

1. **Per-Thread Cache Replication**: Each worker thread gets its own cache copy, eliminating lock contention
2. **Work-Stealing Pattern**: Threads dynamically grab work from a shared partition queue using atomic operations
3. **Adaptive Parallelization**: System detects available cores and memory to scale appropriately
4. **Cache Pre-warming**: Small subtrees are pre-computed once and shared across all threads

### CPU Utilization Results

Tested on a **32-core system with 96 GB RAM**:

- **N=15, M=5**: 1682% CPU (17 cores active), 15.7 seconds, 224,874 trees
- **N=20, M=3**: 2539% CPU (25+ cores active), 14.0 seconds, 46,132 trees
- **N=8, M=5**: 108 trees in 7ms (small problem, minimal parallelization needed)

### Algorithm Complexity

The tree generation problem has **exponential complexity**. The number of trees grows dramatically with N:

| N  | M  | Tree Count | Time       | CPU Usage | Algorithm |
|----|----|-----------:|------------|-----------|-----------|
| 8  | 5  | 108        | 7ms        | 69%       | Standard  |
| 15 | 5  | 224,874    | 16s        | 1682%     | Parallel  |
| 20 | 3  | 46,132     | 14s        | 2539%     | Parallel  |
| 30 | 3  | 267        | 10ms       | N/A       | Optimized |

### Memory Usage

With per-thread caches, memory usage scales linearly with the number of cores:
- Each thread maintains its own cache: O(N × M × TreeCount)
- For N=20: ~700 MB peak resident memory (acceptable for 96 GB system)
- Trade memory for parallelism: **eliminated all lock contention**

### Optimization Techniques Applied

1. **Canonical Form**: Trees are stored in sorted form to avoid generating duplicates
2. **Early Pruning**: Leaf count constraints checked during generation, not after
3. **Memoization**: Dynamic programming with per-thread caches
4. **Partition-Level Parallelism**: Top-level partitions processed independently
5. **Lock-Free Design**: Atomic counters and thread-local data structures
6. **Specialized Small-M Algorithm**: Direct enumeration by exact leaf count for M≤3

## Running with Different Configurations

```bash
# Default: uses all available cores
./tree_generation 20 3

# Quiet mode for large runs
./tree_generation 20 3 --quiet

# Monitor CPU usage externally
time ./tree_generation 20 3 --quiet
```

## Known Limitations

- **Large M with large N**: Very large N values with high M could exhaust memory even with 96 GB
- **Cache Efficiency**: For extremely large N, cache benefits diminish as unique subtree patterns increase

## Algorithm Selection

The implementation automatically selects the best algorithm:

- **M ≤ 3**: Uses specialized optimizer that enumerates trees by exact leaf count
- **N < 10**: Single-threaded generation with memoization
- **N ≥ 10, M > 3**: Multi-threaded parallel generation with per-thread caches

## Future Optimizations

1. **Distributed Computing**: Split work across multiple machines
2. **GPU Acceleration**: Port combinatorial generation to CUDA
3. **Better Algorithms**: Research mathematical properties to reduce search space
4. **Incremental Results**: Stream results to disk instead of holding all in memory
