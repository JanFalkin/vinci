#include "tree_generator.h"
#include "tree_optimizer.h"
#include <algorithm>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <iostream>
#include <format>
#include <chrono>
#include <set>
#ifdef __linux__
#include <sys/sysinfo.h>
#elif __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#elif _WIN32
#include <windows.h>
#include <sysinfoapi.h>
#endif
#include <unistd.h>

namespace vinci {

namespace {
    /**
     * @brief Get available memory in bytes (cross-platform)
     * @return Available memory in bytes, or 0 if unable to determine
     */
    size_t getAvailableMemory() {
#ifdef __linux__
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            return memInfo.freeram;
        }
#elif __APPLE__
        vm_size_t page_size;
        mach_port_t mach_port;
        mach_msg_type_number_t count;
        vm_statistics64_data_t vm_stats;
        
        mach_port = mach_host_self();
        count = sizeof(vm_stats) / sizeof(natural_t);
        if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count) == KERN_SUCCESS) {
            return (vm_stats.free_count + vm_stats.inactive_count) * page_size;
        }
#elif _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            return memInfo.ullAvailPhys;
        }
#endif
        return 0;
    }

    /**
     * @brief Get total system memory in bytes (cross-platform)
     * @return Total memory in bytes, or 0 if unable to determine
     */
    size_t getTotalMemory() {
#ifdef __linux__
        struct sysinfo memInfo;
        if (sysinfo(&memInfo) == 0) {
            return memInfo.totalram;
        }
#elif __APPLE__
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t memsize;
        size_t len = sizeof(memsize);
        if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
            return memsize;
        }
#elif _WIN32
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            return memInfo.ullTotalPhys;
        }
#endif
        return 0;
    }

    /**
     * @brief Check if system has sufficient memory for requested tree generation
     * @param n Number of nodes
     * @param m Maximum leaf nodes
     * @return true if memory is sufficient, false otherwise
     */
    bool checkMemoryAvailability(size_t n, size_t m) {
        size_t availableMemory = getAvailableMemory();
        if (availableMemory == 0) {
            // If we can't get memory info, assume it's fine
            return true;
        }

        // Available memory in GB
        size_t availableMemoryGB = availableMemory / (1024ULL * 1024 * 1024);

        // Conservative memory estimation:
        // - Each tree: ~100 bytes average (string representation + overhead)
        // - Number of trees grows exponentially with N
        // - For N > 20, memory usage can become significant

        // Empirical limits based on OEIS A000081 and testing:
        // N=14: 32,973 trees (~3 MB)
        // N=15: 85,000 trees (~8 MB)
        // N=20: ~1.5M trees (~150 MB)
        // N=25: ~37M trees (~3.7 GB)
        // N=30: ~900M trees (~90 GB) - already problematic

        if (n > 30) {
            std::cerr << std::format(
                "Error: N={} is too large. Maximum supported is N=30.\n"
                "Estimated memory requirement would exceed available memory.\n",
                n
            );
            return false;
        }

        // For N between 25-30, check if we have enough RAM
        if (n >= 25) {
            // Rough exponential estimate: memory ~ 2^(n/3) MB
            size_t estimatedMemoryMB = 1ULL << (n / 3);
            size_t estimatedMemoryGB = estimatedMemoryMB / 1024;

            if (estimatedMemoryGB > availableMemoryGB) {
                std::cerr << std::format(
                    "Error: Insufficient memory for N={}, M={}.\n"
                    "Estimated requirement: ~{} GB\n"
                    "Available memory: ~{} GB\n"
                    "Hint: Try a smaller N value (N <= 20 is safe).\n",
                    n, m, estimatedMemoryGB, availableMemoryGB
                );
                return false;
            }

            // Warn if we're using more than 50% of available memory
            if (estimatedMemoryGB * 2 > availableMemoryGB) {
                std::cerr << std::format(
                    "Warning: N={} may use significant memory (~{} GB).\n"
                    "Available: {} GB. Proceeding, but monitor memory usage...\n",
                    n, estimatedMemoryGB, availableMemoryGB
                );
            }
        }

        return true;
    }
}

size_t TreeGenerator::generate(size_t n, size_t m, TreeCallback callback, bool useMultithreading) {
    count_ = 0;

    // Check memory availability before starting
    if (!checkMemoryAvailability(n, m)) {
        return 0;
    }

    // Initialize cache
    cache_.clear();
    cache_.resize(n + 1);
    for (auto& level : cache_) {
        level.resize(m + 1);
    }

    if (n == 0) {
        return 0;
    }

    // For small cases or when multithreading is disabled, use single-threaded path
    if (!useMultithreading || n < 10) {
        // For small cases, single-threaded is fine
        std::vector<Tree> results;
        generateTreesRecursive(n, m, results, cache_);

        for (const auto& tree : results) {
            callback(tree);
            ++count_;
        }
        return count_;
    }

    // Use parallel optimized algorithm for constrained problems
    if (TreeOptimizer::shouldUseOptimized(n, m)) {
        count_ = TreeOptimizer::generateAllWithCallback(n, m, callback, useMultithreading);
        return count_;
    }

    // Detect system resources
    size_t numCores = std::thread::hardware_concurrency();
    if (numCores == 0) numCores = 4;

    size_t totalMemory = getTotalMemory();
    size_t totalMemoryGB = (totalMemory > 0) ? (totalMemory / (1024ULL * 1024 * 1024)) : 8;

    // Scale parallelism based on resources
    size_t maxThreads = std::min(numCores, size_t(32));
    if (totalMemoryGB > 64) {
        maxThreads = numCores; // Use all cores if we have plenty of RAM
    }

    // Pre-warm cache for small subtrees (single-threaded, shared)
    size_t prewarmSize = std::min(n / 2, size_t(15));
    prewarmCache(prewarmSize, m);

    // Parallel generation strategy:
    // Each thread gets its own cache copy and works on independent partitions
    size_t remainingNodes = n - 1;

    // Generate all partitions first
    // For larger M, we need more partitions to keep threads busy
    size_t maxChildren = std::min(remainingNodes, std::max(size_t(20), m * 5));
    std::vector<std::pair<size_t, std::vector<size_t>>> allPartitions;

    for (size_t numChildren = 1; numChildren <= maxChildren; ++numChildren) {
        std::vector<std::vector<size_t>> partitions;
        std::vector<size_t> current;
        generatePartitions(remainingNodes, numChildren, current, partitions);

        for (auto& partition : partitions) {
            std::sort(partition.begin(), partition.end(), std::greater<size_t>());
            allPartitions.emplace_back(numChildren, partition);
        }
    }

    // Process partitions in parallel with per-thread caches
    std::vector<std::jthread> threads;
    std::vector<std::vector<Tree>> threadResults(maxThreads);
    // Pre-reserve generous space to avoid reallocations during parallel execution
    for (auto& vec : threadResults) {
        vec.reserve(100000);  // Large reservation to prevent any reallocation
    }
    std::vector<std::mutex> resultMutexes(maxThreads);  // One mutex per thread result vector
    std::atomic<size_t> partitionIndex{0};
    std::atomic<size_t> partitionsCompleted{0};
    size_t totalPartitions = allPartitions.size();

    // Pre-create thread caches before launching threads to avoid concurrent copying
    std::vector<std::vector<std::vector<std::vector<Tree>>>> threadCaches(maxThreads);
    for (size_t t = 0; t < maxThreads; ++t) {
        threadCaches[t] = cache_;
    }

    // Launch worker threads
    for (size_t t = 0; t < maxThreads; ++t) {
        threads.emplace_back(
            [this, &allPartitions, &partitionIndex, &partitionsCompleted, &threadResults, &threadCaches, &resultMutexes, t, totalPartitions, n, m, maxThreads](std::stop_token stoken) {
                // Each thread uses its pre-allocated cache
                auto& threadCache = threadCaches[t];

                while (!stoken.stop_requested()) {
                    // Grab work in larger batches to reduce contention
                    size_t batchSize = std::max(size_t(1), allPartitions.size() / (maxThreads * 4));
                    size_t startIdx = partitionIndex.fetch_add(batchSize);
                    size_t endIdx = std::min(startIdx + batchSize, allPartitions.size());

                    if (startIdx >= allPartitions.size()) break;

                    for (size_t idx = startIdx; idx < endIdx; ++idx) {
                        auto& partition = allPartitions[idx].second;

                        // Generate child tree options for this partition
                        std::vector<std::vector<Tree>> childTreeOptions(partition.size());

                        bool validPartition = true;
                        for (size_t i = 0; i < partition.size(); ++i) {
                            generateTreesRecursive(partition[i], m, childTreeOptions[i], threadCache);
                            if (childTreeOptions[i].empty()) {
                                validPartition = false;
                                break;
                            }
                        }

                        if (validPartition) {
                            std::vector<Tree> currentChildren;
                            std::vector<Tree> localResults;
                            generateCombinations(partition, m, childTreeOptions, 0, currentChildren, localResults);
                            
                            // Add results with mutex protection
                            {
                                std::lock_guard<std::mutex> lock(resultMutexes[t]);
                                threadResults[t].insert(threadResults[t].end(), localResults.begin(), localResults.end());
                            }
                        }

                        // Update progress
                        partitionsCompleted.fetch_add(1);
                    }
                }
            }
        );
    }

    // Progress reporting thread (in its own scope to ensure cleanup)
    {
        std::jthread progressThread([&](std::stop_token stoken) {
            auto startTime = std::chrono::steady_clock::now();

            while (!stoken.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));

                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
                size_t currentCount = count_.load();
                size_t completed = partitionsCompleted.load();

                // Always show progress, even if tree count is 0 (still computing)
                if (currentCount > 0) {
                    // Calculate overall rate
                    double overallRate = elapsed > 0 ? static_cast<double>(currentCount) / elapsed : 0.0;

                    std::cout << std::format("\rProgress: {} trees | {}s elapsed | {:.0f} trees/s | Partitions: {}/{}",
                                            currentCount, elapsed, overallRate, completed, totalPartitions)
                              << std::flush;
                } else if (completed > 0 || elapsed > 0) {
                    // Show partition progress even when no trees counted yet
                    std::cout << std::format("\rComputing... {}s elapsed | Partitions: {}/{}",
                                            elapsed, completed, totalPartitions)
                              << std::flush;
                }
            }
        });

        // Wait for all worker threads to complete (RAII auto-joins)
        threads.clear();

        // Collect results with global deduplication
        std::set<std::string> seenGlobal;
        for (auto& trees : threadResults) {
            for (auto& tree : trees) {
                tree.sortToCanonical();
                std::string repr = tree.toString();
                if (seenGlobal.find(repr) == seenGlobal.end()) {
                    seenGlobal.insert(repr);
                    invokeCallback(tree, callback);
                }
            }
        }

        // progressThread stops and joins here when scope exits
    }

    // Now safe to clear the progress line
    std::cout << "\r" << std::string(100, ' ') << "\r" << std::flush;

    return count_;
}

void TreeGenerator::generatePartitions(
    size_t n,
    size_t k,
    std::vector<size_t>& current,
    std::vector<std::vector<size_t>>& result) {

    if (n == 0) {
        result.push_back(current);
        return;
    }

    if (k == 0) {
        return;
    }

    // Determine minimum value to maintain non-increasing order
    size_t minVal = current.empty() ? 1 : 1;
    size_t maxVal = current.empty() ? n : std::min(n, current.back());

    for (size_t i = minVal; i <= maxVal; ++i) {
        current.push_back(i);
        generatePartitions(n - i, k - 1, current, result);
        current.pop_back();
    }
}

void TreeGenerator::prewarmCache(size_t maxN, size_t maxM) {
    // Pre-generate all small subtrees to populate shared cache
    for (size_t n = 1; n <= maxN; ++n) {
        for (size_t m = 1; m <= maxM; ++m) {
            std::vector<Tree> dummy;
            generateTreesRecursive(n, m, dummy, cache_);
        }
    }
}

void TreeGenerator::generateTreesRecursive(size_t n, size_t maxLeaves, std::vector<Tree>& results,
                                           std::vector<std::vector<std::vector<Tree>>>& localCache) {
    // Check cache first (lock-free with per-thread cache)
    if (!localCache[n][maxLeaves].empty()) {
        results = localCache[n][maxLeaves];
        return;
    }

    results.clear();

    // Base case: single node (leaf)
    if (n == 1) {
        if (maxLeaves >= 1) {
            results.push_back(Tree());
        }
        cache_[n][maxLeaves] = results;
        return;
    }

    // Try all possible ways to partition n-1 nodes among children
    // (n-1 because root takes 1 node)
    size_t remainingNodes = n - 1;

    // For large problems, parallelize across different numChildren values
    if (remainingNodes > 12 && maxLeaves > 4) {
        std::vector<std::jthread> threads;
        std::vector<std::vector<Tree>> threadResults(remainingNodes);

        for (size_t numChildren = 1; numChildren <= remainingNodes; ++numChildren) {
            threads.emplace_back(
                [this, remainingNodes, numChildren, maxLeaves, localCache, &threadResults](std::stop_token stoken) mutable {
                    if (stoken.stop_requested()) return;

                    // Generate all partitions of remainingNodes into numChildren parts
                    std::vector<std::vector<size_t>> partitions;
                    std::vector<size_t> current;
                    generatePartitions(remainingNodes, numChildren, current, partitions);

                    for (auto& partition : partitions) {
                        if (stoken.stop_requested()) return;

                        // Sort partition in descending order for canonical form
                        std::sort(partition.begin(), partition.end(), std::greater<size_t>());

                        // For each partition, generate all possible tree combinations
                        std::vector<std::vector<Tree>> childTreeOptions(partition.size());

                        bool validPartition = true;
                        for (size_t i = 0; i < partition.size(); ++i) {
                            // Each child subtree can have at most maxLeaves leaves
                            generateTreesRecursive(partition[i], maxLeaves, childTreeOptions[i], localCache);
                            if (childTreeOptions[i].empty()) {
                                validPartition = false;
                                break;
                            }
                        }

                        if (!validPartition) {
                            continue;
                        }

                        // Generate all combinations of children
                        std::vector<Tree> currentChildren;
                        generateCombinations(partition, maxLeaves, childTreeOptions, 0, currentChildren, threadResults[numChildren - 1]);
                    }
                });
        }

        // Wait for all threads (RAII auto-joins)
        threads.clear();

        // Collect results from all parallel tasks
        for (auto& partialResults : threadResults) {
            results.insert(results.end(), partialResults.begin(), partialResults.end());
        }
    } else {
        // Sequential processing for small problems
        for (size_t numChildren = 1; numChildren <= remainingNodes; ++numChildren) {
            // Generate all partitions of remainingNodes into numChildren parts
            std::vector<std::vector<size_t>> partitions;
            std::vector<size_t> current;
            generatePartitions(remainingNodes, numChildren, current, partitions);

            for (auto& partition : partitions) {
                // Sort partition in descending order for canonical form
                std::sort(partition.begin(), partition.end(), std::greater<size_t>());

                // For each partition, generate all possible tree combinations
                std::vector<std::vector<Tree>> childTreeOptions(partition.size());

                bool validPartition = true;
                for (size_t i = 0; i < partition.size(); ++i) {
                    // Each child subtree can have at most maxLeaves leaves
                    generateTreesRecursive(partition[i], maxLeaves, childTreeOptions[i], localCache);
                    if (childTreeOptions[i].empty()) {
                        validPartition = false;
                        break;
                    }
                }

                if (!validPartition) {
                    continue;
                }

                // Generate all combinations of children
                std::vector<Tree> currentChildren;
                generateCombinations(partition, maxLeaves, childTreeOptions, 0, currentChildren, results);
            }
        }
    }

    // Remove duplicates and ensure canonical form
    std::vector<std::string> seen;
    std::vector<Tree> uniqueResults;

    for (auto& tree : results) {
        tree.sortToCanonical();
        std::string repr = tree.toString();
        if (std::find(seen.begin(), seen.end(), repr) == seen.end()) {
            seen.push_back(repr);
            uniqueResults.push_back(tree);
        }
    }

    results = uniqueResults;
    localCache[n][maxLeaves] = results;
}

void TreeGenerator::generateCombinations(
    const std::vector<size_t>& partition,
    size_t maxLeaves,
    const std::vector<std::vector<Tree>>& childTrees,
    size_t index,
    std::vector<Tree>& current,
    std::vector<Tree>& results) {

    if (index == partition.size()) {
        // Check if total leaf count is within limit
        Tree candidate(current);
        if (candidate.getLeafCount() <= maxLeaves) {
            results.push_back(candidate);
        }
        return;
    }

    // Try all possible trees for the current child position
    for (const auto& tree : childTrees[index]) {
        current.push_back(tree);

        // Early pruning: check if current combination already exceeds leaf limit
        size_t currentLeaves = 0;
        for (const auto& child : current) {
            currentLeaves += child.getLeafCount();
        }

        if (currentLeaves <= maxLeaves) {
            generateCombinations(partition, maxLeaves, childTrees, index + 1, current, results);
        }

        current.pop_back();
    }
}

void TreeGenerator::invokeCallback(const Tree& tree, TreeCallback& callback) {
    if (callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback(tree);
        ++count_;
    }
}

} // namespace vinci
