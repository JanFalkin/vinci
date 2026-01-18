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
     * This is more efficient than the general algorithm for small k
     */
    static void generateWithExactLeaves(
        size_t n,
        size_t k,
        std::vector<Tree>& results
    );

    /**
     * @brief Check if we should use optimized algorithm
     */
    static bool shouldUseOptimized(size_t n, size_t m) {
        return m <= 3 && n >= 20;
    }

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
};

} // namespace vinci
