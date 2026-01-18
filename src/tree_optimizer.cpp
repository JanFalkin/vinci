#include "tree_optimizer.h"
#include <algorithm>
#include <set>

namespace vinci {

void TreeOptimizer::generateWithExactLeaves(size_t n, size_t k, std::vector<Tree>& results) {
    results.clear();

    if (k == 0 || k > n) {
        return;
    }

    if (k == 1) {
        generateSingleLeaf(n, results);
    } else if (k == 2) {
        generateTwoLeaves(n, results);
    } else if (k == 3) {
        generateThreeLeaves(n, results);
    }
}

void TreeOptimizer::generateSingleLeaf(size_t n, std::vector<Tree>& results) {
    if (n == 1) {
        // Single leaf node
        results.push_back(Tree());
        return;
    }

    // Single chain: root -> child -> ... -> leaf
    Tree tree;
    Tree* current = &tree;

    for (size_t i = 1; i < n; ++i) {
        Tree child;
        current->addChild(child);
        if (i < n - 1) {
            // Get reference to the child we just added
            current = const_cast<Tree*>(&current->getChildren()[0]);
        }
    }

    results.push_back(tree);
}

void TreeOptimizer::generateTwoLeaves(size_t n, std::vector<Tree>& results) {
    if (n < 3) {
        return; // Need at least 3 nodes for 2 leaves
    }

    // Strategy: Root has 2 children, each is a chain ending in a leaf
    // Distribute n-1 nodes among the two chains

    size_t remaining = n - 1;

    for (size_t leftChainSize = 1; leftChainSize <= remaining - 1; ++leftChainSize) {
        size_t rightChainSize = remaining - leftChainSize;

        if (rightChainSize < 1) continue;

        // Only generate if leftChainSize >= rightChainSize (canonical form)
        if (leftChainSize < rightChainSize) continue;

        Tree root;

        // Build left chain
        std::vector<Tree> leftChain;
        generateSingleLeaf(leftChainSize, leftChain);

        // Build right chain
        std::vector<Tree> rightChain;
        generateSingleLeaf(rightChainSize, rightChain);

        if (!leftChain.empty() && !rightChain.empty()) {
            root.addChild(rightChain[0]); // Add smaller first for canonical order
            root.addChild(leftChain[0]);
            root.sortToCanonical();
            results.push_back(root);
        }
    }
}

void TreeOptimizer::generateThreeLeaves(size_t n, std::vector<Tree>& results) {
    if (n < 4) {
        return; // Need at least 4 nodes for 3 leaves
    }

    std::set<std::string> seen; // To avoid duplicates
    size_t remaining = n - 1;

    // Case 1: Root has 3 children (each a chain to a leaf)
    for (size_t a = 1; a <= remaining - 2; ++a) {
        for (size_t b = 1; b <= remaining - a - 1; ++b) {
            size_t c = remaining - a - b;
            if (c < 1) continue;

            // Canonical order: a >= b >= c
            std::vector<size_t> sizes = {a, b, c};
            std::sort(sizes.begin(), sizes.end(), std::greater<size_t>());

            if (sizes[0] != a || sizes[1] != b || sizes[2] != c) {
                continue; // Skip non-canonical orderings
            }

            Tree root;
            std::vector<Tree> chain1, chain2, chain3;
            generateSingleLeaf(sizes[0], chain1);
            generateSingleLeaf(sizes[1], chain2);
            generateSingleLeaf(sizes[2], chain3);

            if (!chain1.empty() && !chain2.empty() && !chain3.empty()) {
                root.addChild(chain3[0]); // Smallest first
                root.addChild(chain2[0]);
                root.addChild(chain1[0]); // Largest last
                root.sortToCanonical();

                std::string repr = root.toString();
                if (seen.find(repr) == seen.end()) {
                    seen.insert(repr);
                    results.push_back(root);
                }
            }
        }
    }

    // Case 2: Root has 2 children, one of which has 2 leaves
    for (size_t singleChainSize = 1; singleChainSize <= remaining - 2; ++singleChainSize) {
        size_t twoLeafTreeSize = remaining - singleChainSize;

        if (twoLeafTreeSize < 2) continue;

        std::vector<Tree> singleChain, twoLeafTrees;
        generateSingleLeaf(singleChainSize, singleChain);
        generateTwoLeaves(twoLeafTreeSize, twoLeafTrees);

        for (const auto& twoLeafTree : twoLeafTrees) {
            if (!singleChain.empty()) {
                Tree root;
                root.addChild(singleChain[0]);
                root.addChild(twoLeafTree);
                root.sortToCanonical();

                std::string repr = root.toString();
                if (seen.find(repr) == seen.end()) {
                    seen.insert(repr);
                    results.push_back(root);
                }
            }
        }
    }
}

void TreeOptimizer::generateIntegerPartitions(
    size_t n,
    size_t k,
    size_t minPart,
    std::vector<size_t>& current,
    std::vector<std::vector<size_t>>& result) {

    if (k == 0) {
        if (n == 0) {
            result.push_back(current);
        }
        return;
    }

    if (n == 0) {
        return;
    }

    // Each part must be at least minPart and at most n/k
    size_t maxPart = n - (k - 1) * minPart;
    if (maxPart < minPart) return;

    // Also respect non-increasing order
    if (!current.empty()) {
        maxPart = std::min(maxPart, current.back());
    }

    for (size_t part = minPart; part <= maxPart; ++part) {
        current.push_back(part);
        generateIntegerPartitions(n - part, k - 1, minPart, current, result);
        current.pop_back();
    }
}

} // namespace vinci
