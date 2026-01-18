#include "tree_generator.h"
#include <iostream>
#include <chrono>
#include <iomanip>
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
        count++;
        if (verbose) {
            std::cout << "Tree #" << count << ":\n";
            std::cout << "  Representation: " << tree.toString() << "\n";
            std::cout << "  Nodes: " << tree.getNodeCount()
                      << ", Leaves: " << tree.getLeafCount() << "\n";
            tree.print(std::cout, "  ");
            std::cout << "\n";
        } else {
            // Print progress every 1000 trees
            if (count % 1000 == 0) {
                std::cout << "Generated " << count << " trees so far...\n";
            }
        }
    };

    size_t total = generator.generate(n, m, callback, true);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << std::string(60, '=') << "\n";
    std::cout << "Total trees generated: " << total << "\n";
    std::cout << "Time taken: " << duration.count() << " ms";

    if (duration.count() >= 1000) {
        std::cout << " (" << std::fixed << std::setprecision(2)
                  << duration.count() / 1000.0 << " seconds)";
    }
    std::cout << "\n";

    if (total > 0) {
        double avgTime = static_cast<double>(duration.count()) / total;
        std::cout << "Average time per tree: " << std::fixed << std::setprecision(6)
                  << avgTime << " ms\n";
    }

    return 0;
}
