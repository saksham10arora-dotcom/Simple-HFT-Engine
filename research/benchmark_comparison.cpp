// ════════════════════════════════════════════════════════════════
//  Research Benchmark: 3-Architecture Comparison
//
//  Architecture A: std::map + std::deque     (Tree-based, O(log n))
//  Architecture B: flat array + std::deque   (Array-based, O(1)/O(n))
//  Architecture C: flat array + intrusive LL (Array-based, O(1)/O(1))
//
//  Same workload, same RNG seed, same hardware.
//  Outputs: throughput, latency percentiles, memory overhead.
// ════════════════════════════════════════════════════════════════

#include "OrderBook_v2.hpp"          // Architecture A
#include "OrderBook_v2b.hpp"         // Architecture B
#include "../include/MemoryPool.hpp" // Architecture C (v3.0)
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>
#include <numeric>
#include <algorithm>
#include <string>

// ── Shared workload ────────────────────────────────────────────
struct OrderInput
{
    uint64_t id;
    int64_t  price_ticks;
    uint32_t quantity;
    bool     is_buy;
};

std::vector<OrderInput> generateWorkload(size_t n, uint32_t seed = 12345)
{
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int64_t>  price_dist(9950, 10050);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);

    std::vector<OrderInput> work;
    work.reserve(n);
    for (size_t i = 0; i < n; ++i)
        work.push_back({i, price_dist(rng), qty_dist(rng), (i % 3 != 0)});
    return work;
}

// ── Result struct ──────────────────────────────────────────────
struct BenchResult
{
    std::string name;
    double throughput;     // orders/sec
    double total_us;       // total time in microseconds
    double p50, p90, p99, p999;  // latency percentiles (ns)
    size_t trades;
    size_t final_book_size;
};

// ── Architecture A Benchmark ───────────────────────────────────
BenchResult benchArchA(const std::vector<OrderInput>& work)
{
    arch_a::MemoryPool pool(work.size());
    arch_a::OrderBook book;

    std::vector<arch_a::Order*> orders;
    orders.reserve(work.size());
    for (auto& w : work)
    {
        auto* o = pool.allocate();
        *o = {w.id, w.price_ticks, w.quantity,
              w.is_buy ? arch_a::Side::Buy : arch_a::Side::Sell};
        orders.push_back(o);
    }

    size_t total_trades = 0;
    std::vector<double> latencies;
    latencies.reserve(work.size() / 100);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < orders.size(); ++i)
    {
        bool sample = (i % 100 == 0);
        std::chrono::high_resolution_clock::time_point ts;
        if (sample) ts = std::chrono::high_resolution_clock::now();

        book.addOrder(orders[i]);
        auto trades = book.match();
        total_trades += trades.size();

        if (sample)
        {
            auto te = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(te - ts).count());
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    auto pct = [&](double p) { return latencies[std::min(static_cast<size_t>(p / 100.0 * (n-1)), n-1)]; };

    return {"A: Tree + Deque",
            static_cast<double>(work.size()) / (us / 1e6), us,
            pct(50), pct(90), pct(99), pct(99.9),
            total_trades, book.totalOrders()};
}

// ── Architecture B Benchmark ───────────────────────────────────
BenchResult benchArchB(const std::vector<OrderInput>& work)
{
    arch_b::MemoryPool pool(work.size());
    arch_b::OrderBook book(9000, 11000);

    std::vector<arch_b::Order*> orders;
    orders.reserve(work.size());
    for (auto& w : work)
    {
        auto* o = pool.allocate();
        *o = {w.id, w.price_ticks, w.quantity,
              w.is_buy ? arch_b::Side::Buy : arch_b::Side::Sell};
        orders.push_back(o);
    }

    size_t total_trades = 0;
    std::vector<double> latencies;
    latencies.reserve(work.size() / 100);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < orders.size(); ++i)
    {
        bool sample = (i % 100 == 0);
        std::chrono::high_resolution_clock::time_point ts;
        if (sample) ts = std::chrono::high_resolution_clock::now();

        book.addOrder(orders[i]);
        auto trades = book.match();
        total_trades += trades.size();

        if (sample)
        {
            auto te = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(te - ts).count());
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    auto pct = [&](double p) { return latencies[std::min(static_cast<size_t>(p / 100.0 * (n-1)), n-1)]; };

    return {"B: Array + Deque",
            static_cast<double>(work.size()) / (us / 1e6), us,
            pct(50), pct(90), pct(99), pct(99.9),
            total_trades, book.totalOrders()};
}

// ── Architecture C Benchmark ───────────────────────────────────
BenchResult benchArchC(const std::vector<OrderInput>& work)
{
    hft::MemoryPool<hft::Order> pool(work.size());
    hft::OrderBook book(9000, 11000);

    std::vector<hft::Order*> orders;
    orders.reserve(work.size());
    for (auto& w : work)
    {
        auto* o = pool.allocate();
        *o = {w.id, w.price_ticks, w.quantity,
              w.is_buy ? hft::Side::Buy : hft::Side::Sell};
        orders.push_back(o);
    }

    size_t total_trades = 0;
    std::vector<double> latencies;
    latencies.reserve(work.size() / 100);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < orders.size(); ++i)
    {
        bool sample = (i % 100 == 0);
        std::chrono::high_resolution_clock::time_point ts;
        if (sample) ts = std::chrono::high_resolution_clock::now();

        // Zero-allocation: callback template
        total_trades += book.submitOrder(orders[i], [](const hft::Trade&) {});

        if (sample)
        {
            auto te = std::chrono::high_resolution_clock::now();
            latencies.push_back(std::chrono::duration<double, std::nano>(te - ts).count());
        }
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double us = std::chrono::duration<double, std::micro>(t1 - t0).count();

    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    auto pct = [&](double p) { return latencies[std::min(static_cast<size_t>(p / 100.0 * (n-1)), n-1)]; };

    return {"C: Array + Intrusive",
            static_cast<double>(work.size()) / (us / 1e6), us,
            pct(50), pct(90), pct(99), pct(99.9),
            total_trades, book.totalOrders()};
}

// ── Print Results ──────────────────────────────────────────────
void printResult(const BenchResult& r, double base_throughput)
{
    double speedup = r.throughput / base_throughput;
    std::cout << std::fixed;
    std::cout << "  " << r.name << "\n"
              << "    Throughput:  " << std::setprecision(0) << r.throughput << " orders/sec"
              << "  (" << std::setprecision(2) << speedup << "x)\n"
              << "    Total time:  " << std::setprecision(0) << r.total_us << " us\n"
              << "    Trades:      " << r.trades << "\n"
              << "    Book final:  " << r.final_book_size << " orders\n"
              << "    Latency (ns):\n"
              << "      p50:  " << std::setprecision(0) << r.p50 << "\n"
              << "      p90:  " << r.p90 << "\n"
              << "      p99:  " << r.p99 << "\n"
              << "      p999: " << r.p999 << "\n\n";
}

void printCSV(const std::vector<BenchResult>& results)
{
    std::cout << "=== CSV OUTPUT (copy to Excel/Python) ===\n\n";
    std::cout << "Architecture,Throughput,TotalTime_us,Trades,p50_ns,p90_ns,p99_ns,p999_ns\n";
    for (auto& r : results)
    {
        std::cout << std::fixed << std::setprecision(0)
                  << r.name << ","
                  << r.throughput << ","
                  << r.total_us << ","
                  << r.trades << ","
                  << r.p50 << ","
                  << r.p90 << ","
                  << r.p99 << ","
                  << r.p999 << "\n";
    }
}

// ── Main ───────────────────────────────────────────────────────
int main()
{
    const size_t NUM_ORDERS = 2'000'000;

    std::cout << "\n"
              << "═══════════════════════════════════════════════════\n"
              << "  RESEARCH BENCHMARK: 3-Architecture LOB Comparison\n"
              << "  Orders: " << NUM_ORDERS << " | Seed: 12345\n"
              << "  Price range: 9950-10050 ticks | Qty: 1-100\n"
              << "═══════════════════════════════════════════════════\n\n";

    // Generate shared workload
    std::cout << "Generating workload... ";
    auto work = generateWorkload(NUM_ORDERS);
    std::cout << "done (" << work.size() << " orders)\n\n";

    // Run benchmarks
    std::cout << "Running Architecture A (Tree + Deque)...\n";
    auto rA = benchArchA(work);
    printResult(rA, rA.throughput);

    std::cout << "Running Architecture B (Array + Deque)...\n";
    auto rB = benchArchB(work);
    printResult(rB, rA.throughput);

    std::cout << "Running Architecture C (Array + Intrusive)...\n";
    auto rC = benchArchC(work);
    printResult(rC, rA.throughput);

    // Summary table
    std::cout << "═══════════════════════════════════════════════════\n"
              << "  SUMMARY\n"
              << "═══════════════════════════════════════════════════\n\n";

    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  | Architecture          | Throughput     | Speedup | p50   | p99   |\n"
              << "  |----------------------|----------------|---------|-------|-------|\n"
              << "  | A: Tree + Deque      | " << std::setw(14) << rA.throughput << " | 1.00x   | " << std::setw(5) << rA.p50 << " | " << std::setw(5) << rA.p99 << " |\n"
              << "  | B: Array + Deque     | " << std::setw(14) << rB.throughput << " | " << std::setprecision(2) << std::setw(5) << (rB.throughput / rA.throughput) << "x  | " << std::setprecision(0) << std::setw(5) << rB.p50 << " | " << std::setw(5) << rB.p99 << " |\n"
              << "  | C: Array + Intrusive | " << std::setw(14) << rC.throughput << " | " << std::setprecision(2) << std::setw(5) << (rC.throughput / rA.throughput) << "x  | " << std::setprecision(0) << std::setw(5) << rC.p50 << " | " << std::setw(5) << rC.p99 << " |\n\n";

    // CSV for graphing
    printCSV({rA, rB, rC});

    std::cout << "\n=== Done. Use the CSV above for paper tables/graphs. ===\n\n";

    return 0;
}
