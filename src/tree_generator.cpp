#include "tree_generator.h"
#include <algorithm>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <sys/sysinfo.h>
#include <unistd.h>

namespace vinci {

size_t TreeGenerator::generate(size_t n, size_t m, TreeCallback callback, bool useMultithreading) {
    count_ = 0;

    // Initialize cache
    cache_.clear();
    cache_.resize(n + 1);
    for (auto& level : cache_) {
        level.resize(m + 1);
    }

    if (n == 0) {
        return 0;
    }

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

    // Detect system resources
    size_t numCores = std::thread::hardware_concurrency();
    if (numCores == 0) numCores = 4;

    struct sysinfo memInfo;
    sysinfo(&memInfo);
    size_t totalMemoryGB = memInfo.totalram / (1024 * 1024 * 1024);

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
    std::vector<std::pair<size_t, std::vector<size_t>>> allPartitions;

    for (size_t numChildren = 1; numChildren <= std::min(remainingNodes, size_t(20)); ++numChildren) {
        std::vector<std::vector<size_t>> partitions;
        std::vector<size_t> current;
        generatePartitions(remainingNodes, numChildren, current, partitions);

        for (auto& partition : partitions) {
            std::sort(partition.begin(), partition.end(), std::greater<size_t>());
            allPartitions.emplace_back(numChildren, partition);
        }
    }

    // Process partitions in parallel with per-thread caches
    std::vector<std::future<std::vector<Tree>>> futures;
    std::atomic<size_t> partitionIndex{0};

    // Launch worker threads
    for (size_t t = 0; t < maxThreads; ++t) {
        futures.push_back(std::async(std::launch::async,
            [this, &allPartitions, &partitionIndex, n, m]() -> std::vector<Tree> {
                // Each thread gets its own cache (replicated data)
                std::vector<std::vector<std::vector<Tree>>> threadCache = cache_;
                std::vector<Tree> threadResults;

                while (true) {
                    size_t idx = partitionIndex.fetch_add(1);
                    if (idx >= allPartitions.size()) break;

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
                        generateCombinations(partition, m, childTreeOptions, 0, currentChildren, threadResults);
                    }
                }

                return threadResults;
            }
        ));
    }

    // Collect results as they complete
    for (auto& future : futures) {
        auto trees = future.get();
        for (auto& tree : trees) {
            tree.sortToCanonical();
            callback(tree);
            ++count_;
        }
    }

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

    // Try different numbers of children (at least 1)
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
