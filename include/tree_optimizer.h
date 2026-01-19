#pragma once

#include "tree.h"
#include <vector>
#include <functional>

namespace vinci {

/**
 * @brief Specialized optimizations for tree generation with small M values
 */
class TreeOptimizer {
public:
    using TreeCallback = std::function<void(const Tree&)>;

    /**
     * @brief Generate trees with exactly k leaves and n total nodes
     * Uses generic memoized approach that works for any k
     */
    static void generateWithExactLeaves(
        size_t n,
        size_t k,
        std::vector<Tree>& results
    );

    /**
     * @brief Generic memoized generation for any k
     * Uses cached results from smaller k values
     */
    static void generateWithExactLeavesGeneric(
        size_t n,
        size_t k,
        std::vector<Tree>& results,
        std::vector<std::vector<std::vector<Tree>>>& cache
    );

    /**
     * @brief Build cache in parallel for all (n, k) pairs up to maxN and maxK
     * Uses parallel processing to populate cache efficiently
     */
    static void buildCacheParallel(
        size_t maxN,
        size_t maxK,
        std::vector<std::vector<std::vector<Tree>>>& cache
    );

    /**
     * @brief Check if we should use optimized algorithm
     * Only use for tight leaf constraints where specialized algorithms excel
     */
    static bool shouldUseOptimized(size_t n, size_t m) {
        // Use optimizer only when M is very small (≤4) and N is large enough to benefit
        // For larger M, the parallel generator is more efficient
        return n >= 15 && m <= 4;
    }

    /**
     * @brief Generate all trees with callback (parallel)
     * Efficiently handles any M value with full CPU utilization
     */
    static size_t generateAllWithCallback(
        size_t n,
        size_t maxM,
        const TreeCallback& callback,
        bool showProgress = false
    );

    /**
     * @brief Generate all integer partitions of n into exactly k parts
     * Results in non-increasing order
     */
    static void generateIntegerPartitions(
        size_t n,
        size_t k,
        size_t minPart,
        std::vector<size_t>& current,
        std::vector<std::vector<size_t>>& result
    );

private:
    /**
     * @brief Generate trees with exactly 1 leaf (single chain)
     */
    static void generateSingleLeaf(size_t n, std::vector<Tree>& results);

    /**
     * @brief Generate trees with exactly 2 leaves
     */
    static void generateTwoLeaves(size_t n, std::vector<Tree>& results);

    /**
     * @brief Generate trees with exactly 3 leaves
     */
    static void generateThreeLeaves(size_t n, std::vector<Tree>& results);

    /**
     * @brief Generate trees with exactly 4 leaves
     */
    static void generateFourLeaves(size_t n, std::vector<Tree>& results);
};

} // namespace vinci
