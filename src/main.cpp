#include "../include/MemoryPool.hpp"
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"
#include <iostream>
#include <random>
#include <chrono>
#include <iomanip>

using namespace hft;

int main()
{
    // ── Dashboard ──────────────────────────────────────────────
    const std::string VERSION = "3.0.0";
    std::cout << "==============================================\n"
              << "   ULTRA-LOW LATENCY HFT ENGINE v" << VERSION << "\n"
              << "   Flat-Array LOB | Intrusive Lists | O(1)\n"
              << "   Zero-Alloc Matching | Market/IOC/FOK\n"
              << "==============================================\n\n";

    // ── Setup ──────────────────────────────────────────────────
    const size_t NUM_ORDERS = 1000;
    MemoryPool<Order> pool(NUM_ORDERS * 2); // Extra capacity for demos
    OrderBook book(9000, 11000);            // Tick range: $90.00 - $110.00

    std::mt19937 rng(42);
    std::uniform_int_distribution<int64_t> price_dist(9900, 10100);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);

    // ── Benchmark: Allocation ──────────────────────────────────
    std::vector<Order*> orders;
    orders.reserve(NUM_ORDERS);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < NUM_ORDERS; ++i)
    {
        Order* o = pool.allocate();
        if (!o) break;
        *o = {i, price_dist(rng), qty_dist(rng),
              (i % 3 == 0) ? Side::Sell : Side::Buy};
        orders.push_back(o);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double alloc_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / NUM_ORDERS;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "[BENCH] Avg allocation latency: " << alloc_ns << " ns/order\n\n";

    // ── Benchmark: Submit + Match (zero-alloc callback) ────────
    size_t total_trades = 0;

    auto t2 = std::chrono::high_resolution_clock::now();

    for (auto* o : orders)
    {
        total_trades += book.submitOrder(o, [](const Trade&) {});
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    double match_us = std::chrono::duration<double, std::micro>(t3 - t2).count();
    double throughput = static_cast<double>(NUM_ORDERS) / (match_us / 1e6);

    // ── BBO Snapshot ───────────────────────────────────────────
    std::cout << "--- Live Market State ---\n";
    book.printBBO();
    std::cout << "Orders in book: " << book.totalOrders()
              << " | Bid levels: " << book.bidLevelCount()
              << " | Ask levels: " << book.askLevelCount() << "\n\n";

    // ── Results ────────────────────────────────────────────────
    std::cout << "=== BENCHMARK RESULTS ===\n"
              << "Orders processed: " << NUM_ORDERS << "\n"
              << "Trades executed:  " << total_trades << "\n"
              << "Total time:       " << std::fixed << std::setprecision(2)
              << match_us << " us\n"
              << "Throughput:       " << std::setprecision(0)
              << throughput << " orders/sec\n"
              << "Avg latency:      " << std::setprecision(1)
              << (match_us * 1000.0 / NUM_ORDERS) << " ns/order\n\n";

    // ── Demo: Market Order ─────────────────────────────────────
    std::cout << "--- Market Order Demo ---\n";
    Order* mkt = pool.allocate();
    *mkt = {9000, 0, 25, Side::Buy, OrderType::Market};
    size_t mkt_trades = book.submitOrder(mkt, [](const Trade& t) {
        std::cout << "  TRADE: buy=" << t.buy_order_id
                  << " sell=" << t.sell_order_id
                  << " qty=" << t.quantity
                  << " @" << static_cast<double>(t.price_ticks) / TICK_SIZE << "\n";
    });
    std::cout << "Market buy filled " << mkt_trades << " trade(s), "
              << "remaining: " << mkt->quantity << "\n\n";

    // ── Demo: IOC Order ────────────────────────────────────────
    std::cout << "--- IOC Order Demo ---\n";
    Order* ioc = pool.allocate();
    *ioc = {9001, 9950, 500, Side::Buy, OrderType::IOC};
    size_t ioc_trades = book.submitOrder(ioc, [](const Trade& t) {
        std::cout << "  TRADE: qty=" << t.quantity
                  << " @" << static_cast<double>(t.price_ticks) / TICK_SIZE << "\n";
    });
    std::cout << "IOC buy: " << ioc_trades << " fill(s), "
              << "unfilled: " << ioc->quantity << " (cancelled)\n\n";

    // ── Demo: Cancel & Modify ──────────────────────────────────
    std::cout << "--- Cancel/Modify Demo ---\n";
    if (book.cancelOrder(5))
        std::cout << "Cancelled order #5\n";
    else
        std::cout << "Order #5 already filled or not found\n";

    if (book.modifyOrder(10, 999))
        std::cout << "Modified order #10 qty -> 999\n";
    else
        std::cout << "Order #10 already filled or not found\n";

    book.printBBO();
    std::cout << "\nEngine hot-standby. v3.0 ready.\n";

    return 0;
}