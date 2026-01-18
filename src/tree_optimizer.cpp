#include "tree_optimizer.h"
#include <algorithm>
#include <set>
#include <map>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <iostream>
#include <format>
#include <chrono>

namespace vinci {

size_t TreeOptimizer::generateAllWithCallback(
    size_t n,
    size_t maxM,
    const TreeCallback& callback,
    bool showProgress) {
    
    // Build cache in parallel up to (n, maxM)
    std::vector<std::vector<std::vector<Tree>>> cache(n + 1,
        std::vector<std::vector<Tree>>(maxM + 1));

    if (showProgress) {
        std::cout << "Building cache for N=" << n << ", M=" << maxM << "...\n" << std::flush;
    }

    buildCacheParallel(n, maxM, cache);

    if (showProgress) {
        std::cout << "\rCache built. Generating trees...                    \n" << std::flush;
    }

    // Now iterate through all cached results and call callback
    size_t totalCount = 0;
    for (size_t leafCount = 1; leafCount <= maxM; ++leafCount) {
        const auto& trees = cache[n][leafCount];
        for (const auto& tree : trees) {
            callback(tree);
            ++totalCount;
        }
    }

    return totalCount;
}

void TreeOptimizer::buildCacheParallel(
    size_t maxN,
    size_t maxK,
    std::vector<std::vector<std::vector<Tree>>>& cache) {
    
    size_t numCores = std::thread::hardware_concurrency();
    if (numCores == 0) numCores = 4;

    // Build cache level by level (k=1, then k=2, etc.)
    for (size_t leafCount = 1; leafCount <= maxK; ++leafCount) {
        if (leafCount <= 4) {
            // Use specialized algorithms (fast enough single-threaded)
            for (size_t nodeCount = leafCount; nodeCount <= maxN; ++nodeCount) {
                if (leafCount == 1) {
                    generateSingleLeaf(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 2) {
                    generateTwoLeaves(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 3) {
                    generateThreeLeaves(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 4) {
                    generateFourLeaves(nodeCount, cache[nodeCount][leafCount]);
                }
            }
        } else {
            // Parallel processing with larger batch sizes for better efficiency
            std::vector<std::future<std::vector<std::pair<size_t, std::vector<Tree>>>>> futures;
            
            size_t totalWork = maxN - leafCount + 1;
            // Larger batches = less synchronization, better CPU utilization
            size_t batchSize = std::max(size_t(2), totalWork / numCores);
            std::atomic<size_t> workIndex{leafCount};

            for (size_t t = 0; t < numCores; ++t) {
                futures.push_back(std::async(std::launch::async,
                    [&cache, leafCount, maxN, batchSize, &workIndex]() -> std::vector<std::pair<size_t, std::vector<Tree>>> {
                        std::vector<std::pair<size_t, std::vector<Tree>>> threadResults;
                        
                        while (true) {
                            size_t startN = workIndex.fetch_add(batchSize, std::memory_order_relaxed);
                            if (startN > maxN) break;
                            
                            size_t endN = std::min(startN + batchSize - 1, maxN);
                            
                            for (size_t nodeCount = startN; nodeCount <= endN; ++nodeCount) {
                                std::vector<Tree> results;
                                generateWithExactLeavesGeneric(
                                    nodeCount, leafCount, results, cache);
                                threadResults.emplace_back(nodeCount, std::move(results));
                            }
                        }
                        
                        return threadResults;
                    }));
            }

            // Collect results from all threads
            for (auto& future : futures) {
                auto threadResults = future.get();
                for (auto& [nodeCount, trees] : threadResults) {
                    cache[nodeCount][leafCount] = std::move(trees);
                }
            }
        }
    }
}

void TreeOptimizer::generateWithExactLeaves(size_t n, size_t k, std::vector<Tree>& results) {
    results.clear();

    if (k == 0 || k > n) {
        return;
    }

    // Use specialized algorithms for small k (faster)
    if (k == 1) {
        generateSingleLeaf(n, results);
        return;
    } else if (k == 2) {
        generateTwoLeaves(n, results);
        return;
    } else if (k == 3) {
        generateThreeLeaves(n, results);
        return;
    } else if (k == 4) {
        generateFourLeaves(n, results);
        return;
    }

    // For k >= 5, use generic memoized approach
    // Cache: cache[nodes][leaves] = vector of trees
    std::vector<std::vector<std::vector<Tree>>> cache(n + 1,
        std::vector<std::vector<Tree>>(k + 1));

    // Build up from k=1 to k
    for (size_t leafCount = 1; leafCount <= k; ++leafCount) {
        for (size_t nodeCount = leafCount; nodeCount <= n; ++nodeCount) {
            if (leafCount <= 4) {
                // Use specialized algorithms for small leaf counts
                if (leafCount == 1) {
                    generateSingleLeaf(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 2) {
                    generateTwoLeaves(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 3) {
                    generateThreeLeaves(nodeCount, cache[nodeCount][leafCount]);
                } else if (leafCount == 4) {
                    generateFourLeaves(nodeCount, cache[nodeCount][leafCount]);
                }
            } else {
                generateWithExactLeavesGeneric(nodeCount, leafCount, 
                    cache[nodeCount][leafCount], cache);
            }
        }
    }

    results = cache[n][k];
}

void TreeOptimizer::generateWithExactLeavesGeneric(
    size_t n,
    size_t k,
    std::vector<Tree>& results,
    std::vector<std::vector<std::vector<Tree>>>& cache) {
    
    results.clear();
    std::set<std::string> seen;

    if (k == 0 || k > n || n < k) {
        return;
    }

    size_t remaining = n - 1; // Root accounts for 1 node

    // Try all ways to partition k leaves among children
    // Each child must have at least 1 leaf
    for (size_t numChildren = 1; numChildren <= std::min(k, remaining); ++numChildren) {
        // Partition k leaves into numChildren parts
        std::vector<std::vector<size_t>> leafPartitions;
        std::vector<size_t> currentLeafPart;
        generateIntegerPartitions(k, numChildren, 1, currentLeafPart, leafPartitions);

        for (const auto& leafPart : leafPartitions) {
            // For each leaf partition, try all ways to distribute remaining nodes
            // Each child needs at least as many nodes as leaves
            std::vector<size_t> minNodes = leafPart; // Each child needs at least k_i nodes

            size_t minRequired = 0;
            for (size_t mn : minNodes) minRequired += mn;

            if (remaining < minRequired) continue;

            size_t extraNodes = remaining - minRequired;

            // Distribute extraNodes among numChildren children
            std::vector<std::vector<size_t>> nodeDistributions;
            std::vector<size_t> currentNodeDist;
            generateIntegerPartitions(extraNodes + numChildren, numChildren, 1, 
                currentNodeDist, nodeDistributions);

            for (const auto& nodeDist : nodeDistributions) {
                std::vector<size_t> childNodeCounts(numChildren);
                bool valid = true;
                for (size_t i = 0; i < numChildren; ++i) {
                    childNodeCounts[i] = minNodes[i] + nodeDist[i] - 1;
                    if (childNodeCounts[i] < leafPart[i]) {
                        valid = false;
                        break;
                    }
                }

                if (!valid) continue;

                // Generate all combinations of children
                std::vector<std::vector<Tree>> childOptions(numChildren);
                bool allValid = true;

                for (size_t i = 0; i < numChildren; ++i) {
                    size_t childNodes = childNodeCounts[i];
                    size_t childLeaves = leafPart[i];

                    if (childNodes < childLeaves || childNodes >= cache.size() ||
                        childLeaves >= cache[childNodes].size()) {
                        allValid = false;
                        break;
                    }

                    childOptions[i] = cache[childNodes][childLeaves];
                    if (childOptions[i].empty()) {
                        allValid = false;
                        break;
                    }
                }

                if (!allValid) continue;

                // Generate all combinations
                std::function<void(size_t, std::vector<Tree>&)> buildCombinations =
                    [&](size_t idx, std::vector<Tree>& current) {
                        if (idx == numChildren) {
                            Tree root;
                            for (const auto& child : current) {
                                root.addChild(child);
                            }
                            root.sortToCanonical();

                            std::string repr = root.toString();
                            if (seen.find(repr) == seen.end()) {
                                seen.insert(repr);
                                results.push_back(root);
                            }
                            return;
                        }

                        for (const auto& childTree : childOptions[idx]) {
                            current.push_back(childTree);
                            buildCombinations(idx + 1, current);
                            current.pop_back();
                        }
                    };

                std::vector<Tree> current;
                buildCombinations(0, current);
            }
        }
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

void TreeOptimizer::generateFourLeaves(size_t n, std::vector<Tree>& results) {
    if (n < 4) {
        return; // Need at least 4 nodes for 4 leaves
    }

    results.clear();
    std::set<std::string> seen;
    size_t remaining = n - 1; // Root accounts for 1 node

    // Case 1: Root has 4 children (each a single leaf chain)
    // Partition remaining nodes into 4 chains
    for (size_t a = 1; a <= remaining - 3; ++a) {
        for (size_t b = 1; b <= remaining - a - 2; ++b) {
            for (size_t c = 1; c <= remaining - a - b - 1; ++c) {
                size_t d = remaining - a - b - c;
                if (d < 1) continue;

                // Ensure canonical ordering (non-increasing)
                std::vector<size_t> sizes = {a, b, c, d};
                std::sort(sizes.begin(), sizes.end(), std::greater<size_t>());

                if (sizes[0] != a || sizes[1] != b || sizes[2] != c || sizes[3] != d) {
                    continue; // Skip non-canonical orderings
                }

                Tree root;
                std::vector<Tree> chain1, chain2, chain3, chain4;
                generateSingleLeaf(sizes[0], chain1);
                generateSingleLeaf(sizes[1], chain2);
                generateSingleLeaf(sizes[2], chain3);
                generateSingleLeaf(sizes[3], chain4);

                if (!chain1.empty() && !chain2.empty() && !chain3.empty() && !chain4.empty()) {
                    root.addChild(chain4[0]); // Smallest first
                    root.addChild(chain3[0]);
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
    }

    // Case 2: Root has 3 children: two chains and one 2-leaf tree
    for (size_t chain1Size = 1; chain1Size <= remaining - 3; ++chain1Size) {
        for (size_t chain2Size = 1; chain2Size <= remaining - chain1Size - 2; ++chain2Size) {
            size_t twoLeafTreeSize = remaining - chain1Size - chain2Size;

            if (twoLeafTreeSize < 2) continue;

            std::vector<Tree> singleChain1, singleChain2, twoLeafTrees;
            generateSingleLeaf(chain1Size, singleChain1);
            generateSingleLeaf(chain2Size, singleChain2);
            generateTwoLeaves(twoLeafTreeSize, twoLeafTrees);

            for (const auto& twoLeafTree : twoLeafTrees) {
                if (!singleChain1.empty() && !singleChain2.empty()) {
                    Tree root;
                    root.addChild(singleChain1[0]);
                    root.addChild(singleChain2[0]);
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

    // Case 3: Root has 2 children: one chain and one 3-leaf tree
    for (size_t singleChainSize = 1; singleChainSize <= remaining - 3; ++singleChainSize) {
        size_t threeLeafTreeSize = remaining - singleChainSize;

        if (threeLeafTreeSize < 3) continue;

        std::vector<Tree> singleChain, threeLeafTrees;
        generateSingleLeaf(singleChainSize, singleChain);
        generateThreeLeaves(threeLeafTreeSize, threeLeafTrees);

        for (const auto& threeLeafTree : threeLeafTrees) {
            if (!singleChain.empty()) {
                Tree root;
                root.addChild(singleChain[0]);
                root.addChild(threeLeafTree);
                root.sortToCanonical();

                std::string repr = root.toString();
                if (seen.find(repr) == seen.end()) {
                    seen.insert(repr);
                    results.push_back(root);
                }
            }
        }
    }

    // Case 4: Root has 2 children: two 2-leaf trees
    for (size_t tree1Size = 2; tree1Size <= remaining - 2; ++tree1Size) {
        size_t tree2Size = remaining - tree1Size;

        if (tree2Size < 2) continue;

        std::vector<Tree> twoLeafTrees1, twoLeafTrees2;
        generateTwoLeaves(tree1Size, twoLeafTrees1);
        generateTwoLeaves(tree2Size, twoLeafTrees2);

        for (const auto& tree1 : twoLeafTrees1) {
            for (const auto& tree2 : twoLeafTrees2) {
                Tree root;
                root.addChild(tree1);
                root.addChild(tree2);
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

} // namespace vinci
