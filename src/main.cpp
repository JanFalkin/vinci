#include "tree_generator.h"
#include <iostream>
#include <chrono>
#include <format>
#include <atomic>

using namespace vinci;

int main(int argc, char* argv[]) {
    bool verbose = true;

    if (argc < 3) {
        std::cout << "Usage: " << argv[0] << " <N> <M> [--quiet]\n\n";
        std::cout << "Generate all non-equivalent trees with N nodes and at most M leaves.\n\n";
        std::cout << "Arguments:\n";
        std::cout << "  N         Number of nodes in the tree\n";
        std::cout << "  M         Maximum number of leaf nodes allowed\n";
        std::cout << "  --quiet   Optional: suppress tree output, show only summary\n\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " 8 5\n";
        std::cout << "  " << argv[0] << " 30 3 --quiet\n";
        return 1;
    }

    // Parse command line arguments
    size_t n = std::stoull(argv[1]);
    size_t m = std::stoull(argv[2]);

    if (argc >= 4 && std::string(argv[3]) == "--quiet") {
        verbose = false;
    }

    std::cout << "Generating all trees with N=" << n << " nodes and M≤" << m << " leaves\n";
    std::cout << std::string(60, '=') << "\n\n";

    TreeGenerator generator;
    std::atomic<size_t> count{0};

    auto start = std::chrono::high_resolution_clock::now();

    // Callback to print each tree as it's generated
    auto callback = [&count, verbose](const Tree& tree) {
        size_t current = ++count;
        if (verbose) {
            std::cout << std::format("Tree #{}:\n", current);
            std::cout << std::format("  Representation: {}\n", tree.toString());
            std::cout << std::format("  Nodes: {}, Leaves: {}\n",
                                    tree.getNodeCount(), tree.getLeafCount());
            tree.print(std::cout, "  ");
            std::cout << "\n";
        } else {
            // Print progress every 1000 trees (overwrite same line)
            if (current % 1000 == 0) {
                std::cout << std::format("\rGenerated {} trees so far...", current) << std::flush;
            }
        }
    };

    size_t total = generator.generate(n, m, callback, true);

    // Clear the progress line if we were in quiet mode
    if (!verbose) {
        std::cout << "\r" << std::string(60, ' ') << "\r" << std::flush;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::string(60, '=') << "\n";
    std::cout << std::format("Total trees generated: {}\n", total);
    std::cout << std::format("Time taken: {} ms", duration.count());

    if (duration.count() >= 1000) {
        std::cout << std::format(" ({:.2f} seconds)", duration.count() / 1000.0);
    }
    std::cout << "\n";

    if (total > 0) {
        double avgTime = static_cast<double>(duration.count()) / total;
        std::cout << std::format("Average time per tree: {:.6f} ms\n", avgTime);
    }

    return 0;
}
