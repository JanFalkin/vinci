#include <gtest/gtest.h>
#include "tree.h"

using namespace vinci;

class TreeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TreeTest, EmptyTreeIsLeaf) {
    Tree leaf;
    EXPECT_TRUE(leaf.isLeaf());
    EXPECT_EQ(leaf.getNodeCount(), 1);
    EXPECT_EQ(leaf.getLeafCount(), 1);
}

TEST_F(TreeTest, SingleChildTree) {
    Tree leaf;
    Tree root;
    root.addChild(leaf);

    EXPECT_FALSE(root.isLeaf());
    EXPECT_EQ(root.getNodeCount(), 2);
    EXPECT_EQ(root.getLeafCount(), 1);
}

TEST_F(TreeTest, TwoChildrenTree) {
    Tree leaf1, leaf2;
    Tree root;
    root.addChild(leaf1);
    root.addChild(leaf2);

    EXPECT_FALSE(root.isLeaf());
    EXPECT_EQ(root.getNodeCount(), 3);
    EXPECT_EQ(root.getLeafCount(), 2);
}

TEST_F(TreeTest, DeepTree) {
    // Create a chain: root -> child -> grandchild (leaf)
    Tree grandchild; // leaf
    Tree child;
    child.addChild(grandchild);
    Tree root;
    root.addChild(child);

    EXPECT_EQ(root.getNodeCount(), 3);
    EXPECT_EQ(root.getLeafCount(), 1);
}

TEST_F(TreeTest, ComplexTree) {
    // Root with two children: one leaf and one with two leaf children
    Tree leaf1;

    Tree leaf2, leaf3;
    Tree child2;
    child2.addChild(leaf2);
    child2.addChild(leaf3);

    Tree root;
    root.addChild(leaf1);
    root.addChild(child2);

    EXPECT_EQ(root.getNodeCount(), 5);
    EXPECT_EQ(root.getLeafCount(), 3);
}

TEST_F(TreeTest, CanonicalForm) {
    // Create two topologically equivalent trees with different child orders
    Tree leaf1, leaf2, leaf3;

    // Tree 1: root with [leaf, leaf, leaf] (child3, child2, child1)
    Tree tree1;
    Tree child1_1;
    child1_1.addChild(Tree());
    Tree child1_2;
    child1_2.addChild(Tree());
    child1_2.addChild(Tree());
    tree1.addChild(child1_1);
    tree1.addChild(child1_2);

    // Tree 2: same but different order
    Tree tree2;
    Tree child2_1;
    child2_1.addChild(Tree());
    child2_1.addChild(Tree());
    Tree child2_2;
    child2_2.addChild(Tree());
    tree2.addChild(child2_1);
    tree2.addChild(child2_2);

    tree1.sortToCanonical();
    tree2.sortToCanonical();

    EXPECT_EQ(tree1.toString(), tree2.toString());
}

TEST_F(TreeTest, ToString) {
    Tree leaf;
    EXPECT_EQ(leaf.toString(), "()");

    Tree parent;
    parent.addChild(leaf);
    EXPECT_EQ(parent.toString(), "(())");

    Tree root;
    root.addChild(Tree());
    root.addChild(Tree());
    root.sortToCanonical();
    EXPECT_EQ(root.toString(), "((),())");
}

TEST_F(TreeTest, ComparisonOperators) {
    Tree leaf1, leaf2;
    EXPECT_TRUE(leaf1 == leaf2);

    Tree parent1;
    parent1.addChild(Tree());

    Tree parent2;
    parent2.addChild(Tree());
    parent2.addChild(Tree());

    EXPECT_FALSE(parent1 == parent2);
}
