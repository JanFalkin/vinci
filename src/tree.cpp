#include "tree.h"
#include <algorithm>
#include <sstream>

namespace vinci {

Tree::Tree() : children_() {}

Tree::Tree(const std::vector<Tree>& children) : children_(children) {
    sortToCanonical();
}

void Tree::addChild(const Tree& child) {
    children_.push_back(child);
}

size_t Tree::getNodeCount() const {
    size_t count = 1; // Count this node
    for (const auto& child : children_) {
        count += child.getNodeCount();
    }
    return count;
}

size_t Tree::getLeafCount() const {
    if (children_.empty()) {
        return 1; // This is a leaf
    }

    size_t count = 0;
    for (const auto& child : children_) {
        count += child.getLeafCount();
    }
    return count;
}

void Tree::sortToCanonical() {
    // Recursively sort all children
    for (auto& child : children_) {
        child.sortToCanonical();
    }

    // Sort children by their canonical string representation
    std::sort(children_.begin(), children_.end());
}

std::string Tree::toString() const {
    if (children_.empty()) {
        return "()";
    }

    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < children_.size(); ++i) {
        if (i > 0) oss << ",";
        oss << children_[i].toString();
    }
    oss << ")";
    return oss.str();
}

bool Tree::operator<(const Tree& other) const {
    // Compare by string representation for canonical ordering
    return toString() < other.toString();
}

bool Tree::operator==(const Tree& other) const {
    return toString() == other.toString();
}

void Tree::print(std::ostream& os, const std::string& prefix, bool isLast) const {
    os << prefix;
    os << (isLast ? "└── " : "├── ");
    os << (children_.empty() ? "Leaf" : "Node") << "\n";

    for (size_t i = 0; i < children_.size(); ++i) {
        bool last = (i == children_.size() - 1);
        children_[i].print(os, prefix + (isLast ? "    " : "│   "), last);
    }
}

} // namespace vinci
