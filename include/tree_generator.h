#pragma once

#include "tree.h"
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

namespace vinci {

/**
 * @brief Generates all non-equivalent trees with N nodes and at most M leaves
 */
class TreeGenerator {
public:
    using TreeCallback = std::function<void(const Tree&)>;

    /**
     * @brief Generate all trees with N nodes and at most M leaves
     * @param n Total number of nodes
     * @param m Maximum number of leaf nodes
     * @param callback Function to call for each generated tree
     * @param useMultithreading Enable parallel generation
     * @return Total count of generated trees
     */
    size_t generate(size_t n, size_t m, TreeCallback callback, bool useMultithreading = true);

    /**
     * @brief Get the count of generated trees (thread-safe)
     */
    size_t getCount() const { return count_.load(); }

private:
    std::atomic<size_t> count_{0};
    std::mutex callback_mutex_;
    std::mutex cache_mutex_;

    /**
     * @brief Generate all partitions of n into at most k parts
     * This represents ways to distribute n nodes among children
     */
    void generatePartitions(
        size_t n,
        size_t k,
        std::vector<size_t>& current,
        std::vector<std::vector<size_t>>& result
    );

    /**
     * @brief Recursive tree generation with memoization
     * @param n Number of nodes in subtree
     * @param maxLeaves Maximum leaves allowed in subtree
     * @param results Output vector of generated trees
     * @param localCache Thread-local cache for lock-free operation
     */
    void generateTreesRecursive(
        size_t n,
        size_t maxLeaves,
        std::vector<Tree>& results,
        std::vector<std::vector<std::vector<Tree>>>& localCache
    );

    /**
     * @brief Generate all ways to combine children into a tree
     * Uses Cartesian product with canonical ordering
     */
    void generateCombinations(
        const std::vector<size_t>& partition,
        size_t maxLeaves,
        const std::vector<std::vector<Tree>>& childTrees,
        size_t index,
        std::vector<Tree>& current,
        std::vector<Tree>& results
    );

    /**
     * @brief Pre-warm cache for small values (single-threaded)
     */
    void prewarmCache(size_t maxN, size_t maxM);

    /**
     * @brief Call the callback function in a thread-safe manner
     */
    void invokeCallback(const Tree& tree, TreeCallback& callback);

    // Memoization cache: cache_[n][maxLeaves] = vector of trees
    std::vector<std::vector<std::vector<Tree>>> cache_;
};

} // namespace vinci
