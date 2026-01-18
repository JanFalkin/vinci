#include "tree_generator.h"
#include <iostream>
#include <chrono>
#include <format>
#include <atomic>

using namespace vinci;

int main(int argc, char* argv[]) {
    size_t n = 8;
    size_t m = 5;
    bool verbose = true;

    // Parse command line arguments
    if (argc >= 3) {
        n = std::stoull(argv[1]);
        m = std::stoull(argv[2]);
    }
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
            // Print progress every 1000 trees
            if (current % 1000 == 0) {
                std::cout << std::format("Generated {} trees so far...\n", current);
            }
        }
    };

    size_t total = generator.generate(n, m, callback, true);

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
