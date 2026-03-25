#include "../include/MemoryPool.hpp"
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <iomanip>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace hft;

int main()
{
    std::cout << "\n=== HFT Engine v3.0 Benchmark ===\n\n";

    const size_t NUM_ORDERS = 2'000'000;
    MemoryPool<Order> pool(NUM_ORDERS);
    OrderBook book(9000, 11000);  // $90.00 – $110.00

    std::mt19937 rng(12345);
    std::uniform_int_distribution<int64_t> price_dist(9950, 10050);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);

    // ── Phase 1: Allocation Benchmark ──────────────────────────
    std::vector<Order*> orders;
    orders.reserve(NUM_ORDERS);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ORDERS; ++i)
    {
        Order* o = pool.allocate();
        *o = {i, price_dist(rng), qty_dist(rng),
              (i % 3 == 0) ? Side::Sell : Side::Buy};
        orders.push_back(o);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double alloc_total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double alloc_per_ns   = (alloc_total_us * 1000.0) / NUM_ORDERS;

    std::cout << std::fixed;
    std::cout << "[ALLOC] " << NUM_ORDERS << " orders in "
              << std::setprecision(0) << alloc_total_us << " us  ("
              << std::setprecision(1) << alloc_per_ns << " ns/order)\n";

    // ── Phase 2: Submit + Match (Zero-Alloc Callback) ──────────
    size_t total_trades = 0;

    // Per-order latency sampling (every 100th order)
    std::vector<double> latencies;
    latencies.reserve(NUM_ORDERS / 100);

    auto t2 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ORDERS; ++i)
    {
        bool sample = (i % 100 == 0);
        std::chrono::high_resolution_clock::time_point ts;

        if (sample) ts = std::chrono::high_resolution_clock::now();

        // Zero-allocation submit: empty lambda, no std::vector<Trade>
        total_trades += book.submitOrder(orders[i], [](const Trade&) {});

        if (sample)
        {
            auto te = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(te - ts).count());
        }
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    double match_total_us = std::chrono::duration<double, std::micro>(t3 - t2).count();
    double throughput     = static_cast<double>(NUM_ORDERS) / (match_total_us / 1e6);

    std::cout << "[MATCH] " << NUM_ORDERS << " orders in "
              << std::setprecision(0) << match_total_us << " us  ("
              << std::setprecision(0) << throughput << " orders/sec)\n";
    std::cout << "[MATCH] Trades executed: " << total_trades << "\n";

    // ── Phase 3: Latency Percentiles ───────────────────────────
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();

    auto percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p / 100.0 * (n - 1));
        return latencies[std::min(idx, n - 1)];
    };

    double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / n;

    std::cout << "\n[LATENCY] Sampled " << n << " order cycles:\n"
              << std::setprecision(0)
              << "  Avg:  " << avg  << " ns\n"
              << "  p50:  " << percentile(50)  << " ns\n"
              << "  p90:  " << percentile(90)  << " ns\n"
              << "  p99:  " << percentile(99)  << " ns\n"
              << "  p999: " << percentile(99.9) << " ns\n";

    // ── Phase 4: Deallocation Benchmark ────────────────────────
    auto t4 = std::chrono::high_resolution_clock::now();
    for (auto* o : orders)
        pool.deallocate(o);
    auto t5 = std::chrono::high_resolution_clock::now();
    double dealloc_us = std::chrono::duration<double, std::micro>(t5 - t4).count();

    std::cout << "\n[DEALLOC] " << NUM_ORDERS << " orders in "
              << std::setprecision(0) << dealloc_us << " us  ("
              << std::setprecision(1) << (dealloc_us * 1000.0 / NUM_ORDERS) << " ns/order)\n";

    // ── Summary ────────────────────────────────────────────────
    std::cout << "\n=== Summary ===\n"
              << "Architecture:    Flat-array LOB + Intrusive lists\n"
              << "Matching:        Zero-alloc (callback template)\n"
              << "Pool capacity:   " << pool.capacity() << "\n"
              << "Final book size: " << book.totalOrders() << " orders\n"
              << "Bid levels:      " << book.bidLevelCount() << "\n"
              << "Ask levels:      " << book.askLevelCount() << "\n\n";

    return 0;
}
