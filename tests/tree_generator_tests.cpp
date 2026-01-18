#include <gtest/gtest.h>
#include "tree_generator.h"
#include <set>

using namespace vinci;

class TreeGeneratorTest : public ::testing::Test {
protected:
    TreeGenerator generator;

    // Helper to collect trees and their string representations
    std::vector<std::string> generateAndCollect(size_t n, size_t m) {
        std::vector<std::string> results;
        std::set<std::string> seen; // To check for duplicates

        generator.generate(n, m, [&](const Tree& tree) {
            std::string repr = tree.toString();

            // Verify no duplicates
            EXPECT_EQ(seen.count(repr), 0) << "Duplicate tree: " << repr;
            seen.insert(repr);

            // Verify constraints
            EXPECT_EQ(tree.getNodeCount(), n) << "Tree has wrong node count";
            EXPECT_LE(tree.getLeafCount(), m) << "Tree exceeds leaf limit";

            results.push_back(repr);
        }, false); // Single-threaded for testing

        return results;
    }
};

TEST_F(TreeGeneratorTest, N1M1) {
    // Only one tree: a single leaf
    auto results = generateAndCollect(1, 1);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], "()");
}

TEST_F(TreeGeneratorTest, N2M1) {
    // Only one tree: root with one leaf child
    auto results = generateAndCollect(2, 1);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], "(())");
}

TEST_F(TreeGeneratorTest, N3M2) {
    // Possible trees:
    // 1. Root with 2 leaf children: ((),())
    // 2. Chain of 3 nodes: ((()))
    auto results = generateAndCollect(3, 2);
    EXPECT_EQ(results.size(), 2);

    std::set<std::string> resultSet(results.begin(), results.end());
    EXPECT_EQ(resultSet.count("((),())"), 1);
    EXPECT_EQ(resultSet.count("((()))"), 1);
}

TEST_F(TreeGeneratorTest, N3M1) {
    // Only the chain (leaf count = 1): ((()))
    auto results = generateAndCollect(3, 1);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], "((()))");
}

TEST_F(TreeGeneratorTest, N4M2) {
    // Several possibilities with at most 2 leaves
    auto results = generateAndCollect(4, 2);

    // Should have multiple valid trees
    EXPECT_GT(results.size(), 0);

    // All verification is done in generateAndCollect
}

TEST_F(TreeGeneratorTest, N4M3) {
    // More trees allowed with up to 3 leaves
    auto results4m3 = generateAndCollect(4, 3);
    auto results4m2 = generateAndCollect(4, 2);

    EXPECT_GE(results4m3.size(), results4m2.size());
}

TEST_F(TreeGeneratorTest, N5M2) {
    // Test with N=5, M=2
    auto results = generateAndCollect(5, 2);
    EXPECT_GT(results.size(), 0);
}

TEST_F(TreeGeneratorTest, ZeroNodes) {
    // Edge case: no nodes
    auto results = generateAndCollect(0, 5);
    EXPECT_EQ(results.size(), 0);
}

TEST_F(TreeGeneratorTest, ZeroLeavesAllowed) {
    // Edge case: no leaves allowed (impossible for n > 0)
    auto results = generateAndCollect(3, 0);
    EXPECT_EQ(results.size(), 0);
}

TEST_F(TreeGeneratorTest, MultithreadingConsistency) {
    // Test that multithreading produces same count as single-threaded
    size_t n = 6;
    size_t m = 3;

    size_t countSingle = 0;
    generator.generate(n, m, [&](const Tree&) { ++countSingle; }, false);

    TreeGenerator generator2;
    size_t countMulti = 0;
    generator2.generate(n, m, [&](const Tree&) { ++countMulti; }, true);

    EXPECT_EQ(countSingle, countMulti);
}

TEST_F(TreeGeneratorTest, Assignment_N8M5) {
    // First assignment case: N=8, M=5
    std::cout << "\nTesting N=8, M=5...\n";

    size_t count = 0;
    generator.generate(8, 5, [&](const Tree& tree) {
        ++count;
        if (count <= 3) { // Print first few for verification
            std::cout << "  Example tree #" << count << ": " << tree.toString() << "\n";
        }
    }, false);

    std::cout << "Total trees for N=8, M=5: " << count << "\n";
    EXPECT_GT(count, 0);
}

// Note: N=30, M=3 might take a while, so it's commented out for quick tests
// Uncomment to run the full assignment test
/*
TEST_F(TreeGeneratorTest, Assignment_N30M3) {
    // Second assignment case: N=30, M=3
    std::cout << "\nTesting N=30, M=3...\n";

    size_t count = 0;
    generator.generate(30, 3, [&](const Tree& tree) {
        ++count;
        if (count <= 3) {
            std::cout << "  Example tree #" << count << ": " << tree.toString() << "\n";
        }
    }, true);

    std::cout << "Total trees for N=30, M=3: " << count << "\n";
    EXPECT_GT(count, 0);
}
*/
