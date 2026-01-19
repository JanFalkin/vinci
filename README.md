# Tree Generation - Vinci Take-Home Assignment

A C++20 implementation for generating all non-equivalent trees with N nodes and at most M leaves.

## Problem Description

Generate all possible trees with N nodes that have at most M leaf nodes, where:
- Trees can have any number of children (not restricted to binary trees)
- Topologically equivalent trees are not generated redundantly
- Trees are considered equivalent if children can be reordered to match

## Build Requirements

- CMake 3.14 or higher
- C++20 compatible compiler (GCC 10+, Clang 10+, or MSVC 2019 16.10+)
- Internet connection (for fetching Google Test)

## Building the Project

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .
```

## Python script
A Python script `run_tests.py` is provided to automate running tests and displaying results:

```bash
python3 run_tests.py --verbose
```
*Note `run_tests.py` will also build the binary and run all the test.*
```bash
./run_tests.py --help
usage: run_tests.py [-h] [--build-dir BUILD_DIR] [--filter FILTER] [--no-build] [--verbose] [--no-color]

Build and run Vinci tree generator tests

options:
  -h, --help            show this help message and exit
  --build-dir BUILD_DIR
                        Build directory (default: build)
  --filter FILTER       Google Test filter pattern (e.g., "*OEIS*")
  --no-build            Skip building, just run tests
  --verbose             Show detailed build output and warnings
  --no-color            Disable colored output

Examples:
  run_tests.py                          # Build and run all tests
  run_tests.py --filter "*OEIS*"        # Run only OEIS tests
  run_tests.py --no-build               # Skip build, just run tests
  run_tests.py --verbose                # Show detailed output
```


## Running the Main Program

```bash
# Run with custom values
./tree_generation <N> <M> [--quiet]

# Examples:
./tree_generation 8 5                    # Generate N=8, M=5 with verbose output
./tree_generation 30 3 --quiet           # Generate N=30, M=3 quietly
```

**Arguments:**
- `N`: Number of nodes in the tree
- `M`: Maximum number of leaf nodes allowed
- `--quiet`: Optional flag to suppress tree output, show only summary

## Running Tests

The project includes 22 comprehensive tests covering:
- Basic tree operations and canonical form
- Algorithm correctness for various N and M values
- **OEIS A000081 validation**: Verifies counts match the authoritative sequence for rooted trees (N=1-14)

```bash
# Run all tests (using Python runner)
./run_tests.py

# Or use CMake directly
cd build && ctest --output-on-failure

# Or run the test executable directly
./tree_tests

# Run only OEIS validation tests
./tree_tests --gtest_filter="*OEIS*"
```

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── run_tests.py
├── include/
│   ├── tree.h
│   ├── tree_generator.h
│   └── tree_optimizer.h
├── src/
│   ├── main.cpp
│   ├── tree.cpp
│   ├── tree_generator.cpp
│   └── tree_optimizer.cpp
└── tests/
    ├── tree_tests.cpp
    └── tree_generator_tests.cpp
```

## Implementation Details

### Key Features

1. **Canonical Form**: Trees are stored in canonical form to avoid generating topologically equivalent trees
2. **Memoization**: Dynamic programming with caching for efficient generation
3. **Multithreading**: Parallel processing of results when beneficial
4. **Early Pruning**: Leaf count constraints are checked during generation to avoid invalid branches
5. **Memory Safety**: Pre-flight checks prevent OOM crashes for oversized requests (N > 30)

### Algorithm

The generator uses a recursive approach:
1. For N nodes, partition (N-1) nodes among children
2. For each partition, recursively generate all valid subtrees
3. Combine subtrees ensuring canonical ordering
4. Use memoization to avoid recomputing identical subproblems

## Assignment Test Cases

The code solves both required test cases efficiently:

1. **N=8, M=5**: ✅ Generates 108 trees in ~8ms
2. **N=30, M=3**: ✅ Generates 267 trees in ~70ms (using specialized algorithm for M≤4)

### Performance on 32-Core System

- **N=8, M=5**: 108 trees in ~8ms
- **N=30, M=3**: 267 trees in ~70ms (using specialized TreeOptimizer for M≤4)
- **N=14, M=50**: 32,973 trees in ~26s with multi-core parallelization

The implementation automatically uses all available CPU cores, achieving ~9 cores active (868% CPU utilization) on compute-intensive workloads.

## Multithreading Implementation

The solution uses aggressive parallelization optimized for high-core-count systems:

- **Per-Thread Cache Replication**: Each thread maintains its own memoization cache, eliminating lock contention
- **Work-Stealing Pattern**: Threads dynamically grab work using atomic operations
- **System Resource Detection**: Automatically scales parallelism based on available CPU cores and RAM
- **Cache Pre-warming**: Pre-computes small subtrees to accelerate generation
- **Memory Safety Checks**: Validates available system memory before starting computation to prevent OOM crashes

The implementation checks system memory and prevents execution when:
- N > 30 (tree count grows exponentially beyond practical limits)
- Estimated memory usage exceeds available RAM
- For N ≥ 25, displays warnings if memory usage will be significant

On a 32-core system with 96 GB RAM, the implementation achieves **~9 cores active** (868% CPU usage for N=14,M=50 workload).

## GPU Considerations

### Why CPU is Optimal for This Problem

While GPU acceleration might seem attractive for combinatorial problems, this specific algorithm is **CPU-optimal** due to several architectural considerations:

**Problem characteristics that favor CPU:**

1. **Irregular branching**: Different partitions generate vastly different numbers of trees (some generate millions, others generate 10). GPUs suffer significant performance degradation with divergent control flow, as all threads in a warp must follow the same execution path.

2. **Deep recursion**: The algorithm uses recursion up to N levels deep. GPUs have limited stack space (~2KB per thread) and poor support for deep recursion, requiring manual conversion to iterative approaches.

3. **Dynamic memory allocation**: Tree objects are created dynamically during generation. GPUs are inefficient at runtime memory allocation within kernels.

4. **String operations**: Canonical form requires child sorting and string comparisons - operations that CPUs handle natively but GPUs struggle with.

5. **Memoization with complex cache**: The multi-level cache (N × M) benefits from CPU cache hierarchy. GPU memory models don't efficiently support this access pattern.

**Current CPU solution advantages:**

- Achieved **868% CPU utilization** (~9 cores actively working) with good parallel efficiency
- Native support for recursion, dynamic allocation, and string operations
- Efficient L1/L2/L3 cache hierarchy for memoization
- No PCIe transfer overhead for results
- Simpler implementation and debugging

**When GPU would be appropriate:**

GPUs excel at problems with:
- Regular, uniform computation (matrix operations, image convolution)
- Minimal branching (same code path for all threads)
- Dense numerical workloads (floating-point operations)
- Independent parallel tasks (Monte Carlo simulations, ray tracing)

For problems like counting/enumerating mathematical structures with constraints, highly-parallel CPU architectures are typically the better choice.

## Author

Jan Falkin
