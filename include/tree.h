#pragma once

#include <vector>
#include <string>
#include <memory>
#include <ostream>

namespace vinci {

/**
 * @brief Represents a tree node with arbitrary number of children
 */
class Tree {
public:
    Tree();
    explicit Tree(const std::vector<Tree>& children);

    // Add a child to this tree
    void addChild(const Tree& child);

    // Get the children of this tree
    const std::vector<Tree>& getChildren() const { return children_; }

    // Get the number of nodes in the tree (including root)
    size_t getNodeCount() const;

    // Get the number of leaf nodes in the tree
    size_t getLeafCount() const;

    // Check if this tree is a leaf
    bool isLeaf() const { return children_.empty(); }

    // Sort children to canonical form for equivalence checking
    void sortToCanonical();

    // String representation for printing and comparison
    std::string toString() const;

    // Comparison operators for canonical form
    bool operator<(const Tree& other) const;
    bool operator==(const Tree& other) const;

    // Print tree in a readable format
    void print(std::ostream& os, const std::string& prefix = "", bool isLast = true) const;

private:
    std::vector<Tree> children_;
};

} // namespace vinci
