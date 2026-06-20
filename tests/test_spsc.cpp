// ── Concurrency correctness test for the lock-free SPSC ring ─────────────
//
// Producer pushes 0..N-1 on one thread; consumer pops on another. We assert:
//   * exactly N items come out (nothing lost, nothing duplicated),
//   * they come out in order (SPSC is FIFO),
//   * the checksum matches.
//
// Build clean, then run under ThreadSanitizer to prove no data races:
//   clang++ -std=c++17 -O1 -g -fsanitize=thread tests/test_spsc.cpp \
//           -Iinclude -o /tmp/test_spsc_tsan && /tmp/test_spsc_tsan
//
#include "../include/SPSCRing.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>

using namespace hft;

int main()
{
    constexpr size_t N = 5'000'000;
    SPSCRing<uint64_t> ring(1u << 14); // small ring on purpose: forces full/empty churn
    std::atomic<bool>  done{false};

    std::thread producer([&] {
        for (uint64_t i = 0; i < N; ++i)
            while (!ring.try_push(i))
                std::this_thread::yield();
        done.store(true, std::memory_order_release);
    });

    uint64_t count    = 0;
    uint64_t expected = 0; // next value we expect (proves FIFO ordering)
    uint64_t checksum = 0;
    bool     ordered  = true;

    std::thread consumer([&] {
        uint64_t v;
        auto take = [&](uint64_t x) {
            if (x != expected) ordered = false;
            ++expected;
            checksum += x;
            ++count;
        };
        while (!done.load(std::memory_order_acquire))
            if (ring.try_pop(v)) take(v);
        while (ring.try_pop(v)) take(v); // drain
    });

    producer.join();
    consumer.join();

    const uint64_t want_sum = (N - 1) * N / 2; // sum of 0..N-1
    const bool ok = (count == N) && ordered && (checksum == want_sum);

    std::cout << "items popped : " << count    << " (expected " << N << ")\n"
              << "fifo ordered : " << (ordered ? "yes" : "NO") << "\n"
              << "checksum     : " << (checksum == want_sum ? "match" : "MISMATCH") << "\n"
              << (ok ? "PASS: SPSC ring is correct under concurrency\n"
                     : "FAIL\n");
    return ok ? 0 : 1;
}
