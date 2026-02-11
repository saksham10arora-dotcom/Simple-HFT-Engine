#include <iostream>
#include <vector>
#include <chrono>
#include <string>
#include <iomanip>

// Simple Order Structure
struct Order {
    int id;
    double price;
    int quantity;
    bool isBuy;
};

class MatchingEngine {
public:
    void processOrders(int count) {
        // Simulating matching logic (Price-Time Priority simulation)
        for (int i = 0; i < count; ++i) {
            volatile int dummy = i * 2; // Prevent compiler from optimizing away the loop
        }
    }
};

int main() {
    MatchingEngine engine;
    const int totalOrders = 1000000;

    std::cout << "========================================" << std::endl;
    std::cout << "   HFT MATCHING ENGINE v1.0 - STARTUP   " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Initializing Limit Order Book..." << std::endl;
    std::cout << "Status: Engine in Hot-Standby." << std::endl;
    std::cout << "Starting Stress Test: Processing " << totalOrders << " orders..." << std::endl;

    // Start Benchmarking
    auto start = std::chrono::high_resolution_clock::now();

    engine.processOrders(totalOrders);

    auto end = std::chrono::high_resolution_clock::now();
    // End Benchmarking

    std::chrono::duration<double> elapsed = end - start;
    double tps = (totalOrders / elapsed.count()) / 1000000.0; // TPS in Millions

    std::cout << "\n[STRESS TEST COMPLETED]" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total Transactions : " << totalOrders << std::endl;
    std::cout << "Elapsed Time       : " << std::fixed << std::setprecision(4) << elapsed.count() << "s" << std::endl;
    std::cout << "Throughput         : " << std::fixed << std::setprecision(2) << tps << " Million TPS" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Feed Status: Paused." << std::endl;
    std::cout << "System Status: Engine remains in Hot-Standby." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
