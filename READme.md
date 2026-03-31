# HFT Matching Engine

A limit order book matching engine I built in C++ to learn how real exchanges work under the hood.

The idea was simple: take the core data structure of every exchange (the order book) and figure out how to make it fast. Not "fast for a student project" fast — actually fast, in terms of cache behavior and zero-allocation hot paths.

## What it does

- Maintains a two-sided order book (bids and asks)
- Matches incoming orders against resting orders using price-time priority
- Supports Limit, Market, IOC, and FOK order types
- Cancel and modify in O(1)
- Reports trades via zero-allocation callback

## How it evolved

This project went through a few rewrites. The first version used `std::map` for price levels and `std::deque` for order queues. It worked, but the performance was mediocre and the benchmark was honestly kind of fake — it was timing an empty loop, not real matching.

v3 (current) is a full rewrite:

| What | Before | After |
|------|--------|-------|
| Price levels | `std::map` (red-black tree) | Flat array indexed by tick |
| Order queues | `std::deque` | Intrusive doubly-linked list |
| Trade output | `std::vector<Trade>` return | Callback template (inlined) |
| Memory | `std::vector<Order>` (default-constructed) | Raw `char*` arena + free list |
| Pricing | `double` | `int64_t` ticks |

The result was about 3x faster on the same workload.

## Project structure

```
include/
  Order.hpp         - 40-byte order struct, fits in one cache line
  Trade.hpp         - execution report (POD)
  PriceLevel.hpp    - intrusive linked list: push_back, remove, pop_front
  MemoryPool.hpp    - bump-pointer arena with free-list recycling
  OrderBook.hpp     - the engine: flat arrays, matching, submit/cancel/modify

src/
  main.cpp          - demo + benchmark

tests/
  test_engine.cpp   - 21 unit tests
  benchmark.cpp     - 2M order throughput and latency percentiles

research/
  benchmark_comparison.cpp  - runs all 3 architectures on same workload
  OrderBook_v2.hpp          - tree-based implementation (for comparison)
  OrderBook_v2b.hpp         - array + deque (intermediate step)
```

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the engine:
```bash
./build/Release/hft_engine.exe
```

Run tests:
```bash
./build/Release/test_engine.exe     # 21 tests
./build/Release/benchmark.exe       # 2M order benchmark
./build/Release/bench_compare.exe   # 3-architecture comparison
```

## Performance

Numbers from `bench_compare.exe` on my machine (2M orders, same seed, Release mode):

| Architecture | Throughput | p50 | p99 | p999 |
|---|---|---|---|---|
| Tree + Deque | 861K/s | 800ns | 3000ns | 45μs |
| Array + Deque | 1.36M/s | 500ns | 2200ns | 24μs |
| Array + Intrusive | 2.57M/s | 300ns | 900ns | 3.4μs |

The big takeaway wasn't the absolute numbers (which depend on hardware), but the relative improvement and especially the tail latency. p999 going from 45μs to 3.4μs was the most satisfying part — that's a 13x reduction in worst-case latency.

## Key design decisions

**Integer ticks instead of floats.** My first version used `double` for prices. Then I hit the classic `0.1 + 0.2 != 0.3` problem during matching. Switched to integer ticks — every real exchange does this for a reason.

**Flat array instead of tree.** `std::map` gives you O(log n) lookup but every node is at a random memory address. A flat array gives O(1) and the price levels sit next to each other in memory, so the CPU prefetcher actually helps you. The tradeoff is wasted memory for empty levels, but with a reasonable tick range it's fine.

**Intrusive linked lists.** This was the change that made the biggest difference. With `std::deque`, cancelling an order means scanning the whole queue to find it — O(n). With intrusive lists, each order carries its own prev/next pointers, so removal is just pointer rewiring — O(1). The downside is +16 bytes per order, but we still fit in a cache line.

**Callback template for trade output.** Instead of building a `vector<Trade>` on every match call (which means malloc on the hot path), matching takes a callback and calls it for each trade. The compiler inlines the lambda, so in the benchmark case `[](const Trade&) {}` literally compiles to nothing.

## Lessons learned

1. **Measure before optimizing.** My v1 "benchmark" was timing an empty loop. I was proud of 2 billion TPS until I realized no actual matching was happening. Embarrassing, but a good lesson — always measure what matters.

2. **Cache effects dominate algorithmic complexity.** O(1) vs O(log n) matters less than you'd think when n is small. The real win from flat arrays was cache locality, not algorithmic complexity. I only understood this after reading "What Every Programmer Should Know About Memory" by Ulrich Drepper.

3. **The hot path is everything.** In HFT, you optimize the path that runs millions of times per second. Everything else (startup, shutdown, config) can be slow. This mindset changed how I structured the code — the matching loop touches nothing except the order book arrays and the intrusive list.

4. **Don't mix concerns in data structures.** My v1 Order struct had both application data (price, quantity) and container data (position in a vector). Intrusive lists let the Order carry its own list membership, which is cleaner and faster.

5. **Integer arithmetic is underrated.** Switching from float to integer pricing fixed two bugs I had been chasing for days. When you're comparing prices millions of times, even tiny floating point errors compound.

6. **Start ugly, make it fast, then make it clean.** My v1 was ugly but functional. v2 was cleaner but slow in wrong places. v3 got both right. Trying to write perfect code on the first attempt would have paralyzed me.

## What I'd do differently

- Add proper logging with nanosecond timestamps
- Implement a FIX protocol parser for realistic message input
- Add multi-symbol support (currently single-symbol)
- Try lock-free queues for multi-threaded feeding
- Profile with Intel VTune instead of just wall-clock timing

## Tech

- C++17
- CMake
- No external dependencies
- Tested on MSVC 17.x (Windows 11)