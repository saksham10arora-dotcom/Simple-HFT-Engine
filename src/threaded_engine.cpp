// ── Multithreaded matching engine ────────────────────────────────────────
//
// A feed (producer) thread and a matching (consumer) thread, decoupled by a
// lock-free SPSC ring. This is what makes the engine genuinely concurrent:
// the producer never blocks on the matcher, and they share no locks.
//
// The order MemoryPool stays thread-local to the matcher, so the pool is never
// touched by two threads at once. Orders cross the thread boundary as a small
// POD message (no pointers), which is what makes them safe to copy through the
// ring.
//
//   feed thread  ──push(OrderMsg)──▶  [ SPSC ring ]  ──pop──▶  matching thread
//
#include "../include/MemoryPool.hpp"
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"
#include "../include/SPSCRing.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>

using namespace hft;

// POD that crosses the thread boundary. No pointers -> trivially copyable.
struct OrderMsg
{
    uint64_t  id;
    int64_t   price_ticks;
    uint32_t  quantity;
    Side      side;
    OrderType type;
};

int main()
{
    constexpr size_t   NUM_ORDERS = 2'000'000;
    constexpr size_t   RING_CAP   = 1u << 16; // 65,536 slots (power of two)

    SPSCRing<OrderMsg> ring(RING_CAP);
    std::atomic<bool>  feed_done{false};

    std::cout << "=============================================\n"
              << "   MULTITHREADED HFT ENGINE (lock-free SPSC)\n"
              << "=============================================\n"
              << "Orders: " << NUM_ORDERS << " | ring: " << RING_CAP << " slots\n\n";

    const auto t0 = std::chrono::high_resolution_clock::now();

    // ── Feed (producer) thread ───────────────────────────────────────────
    std::thread feed([&] {
        std::mt19937                            rng(42);
        std::uniform_int_distribution<int64_t>  price(9900, 10100);
        std::uniform_int_distribution<uint32_t> qty(1, 100);

        for (size_t i = 0; i < NUM_ORDERS; ++i)
        {
            OrderMsg m{i, price(rng), qty(rng),
                       (i % 3 == 0) ? Side::Sell : Side::Buy,
                       OrderType::Limit};
            // Spin until there is room; the matcher is draining concurrently.
            while (!ring.try_push(m))
                std::this_thread::yield();
        }
        feed_done.store(true, std::memory_order_release);
    });

    // ── Matching (consumer) thread ───────────────────────────────────────
    size_t processed = 0;
    size_t trades    = 0;

    std::thread matcher([&] {
        MemoryPool<Order> pool(NUM_ORDERS + 16); // thread-local: no cross-thread pool race
        OrderBook         book(9000, 11000);
        OrderMsg          m;

        auto handle = [&](const OrderMsg& msg) {
            Order* o = pool.allocate();
            if (!o) return; // pool sized for the full run; should never trigger
            *o = {msg.id, msg.price_ticks, msg.quantity, msg.side, msg.type};
            trades += book.submitOrder(o, [](const Trade&) {});
            ++processed;
        };

        // Run until the feed signals completion, then drain whatever is left.
        while (!feed_done.load(std::memory_order_acquire))
        {
            if (ring.try_pop(m)) handle(m);
            else                 std::this_thread::yield();
        }
        while (ring.try_pop(m)) handle(m); // drain remainder
    });

    feed.join();
    matcher.join();

    const auto t1 = std::chrono::high_resolution_clock::now();
    const double secs = std::chrono::duration<double>(t1 - t0).count();
    const double tput = static_cast<double>(processed) / secs;

    std::cout << std::fixed << std::setprecision(0);
    std::cout << "Processed:   " << processed << " orders\n"
              << "Trades:      " << trades << "\n"
              << "Wall time:   " << std::setprecision(3) << secs << " s\n"
              << "Throughput:  " << std::setprecision(0) << tput
              << " quote updates/sec (2 threads, lock-free)\n";

    if (processed != NUM_ORDERS)
    {
        std::cerr << "\nERROR: lost orders across the ring ("
                  << processed << " != " << NUM_ORDERS << ")\n";
        return 1;
    }
    std::cout << "\nOK: every order crossed the ring exactly once.\n";
    return 0;
}
