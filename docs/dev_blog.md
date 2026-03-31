# How I Built an HFT Matching Engine (And All The Things I Got Wrong)

I'm a second-year CSE undergrad. A few months ago I decided to build a limit order book matching engine in C++ — the kind that sits inside exchanges and matches buy/sell orders.

Here's the honest story of how it went from a fake benchmark to a real system processing 2.7 million orders per second.

---

## The starting point: I had no idea what I was doing

I knew I wanted to build "something HFT" because it sounded impressive. I didn't really understand what an order book was. I Googled "limit order book C++" and cobbled together a basic version:

- `std::map` for storing price levels (because sorted, right?)
- `std::vector` for storing orders at each price
- `double` for prices (because money has decimals, right?)

It compiled. It ran. I was proud.

Then I wrote a "stress test" that looked like this:

```cpp
void processOrders(int count) {
    for (int i = 0; i < count; ++i) {
        volatile int dummy = i * 2;
    }
}
```

That's not a stress test. That's a for loop that multiplies by 2. Nothing touches the order book. But it said "2.5 billion TPS" and I put it on LinkedIn.

**Lesson 1: If your benchmark seems too good to be true, it probably is.**

---

## The first real bug: floating point prices

Once I started actually matching orders, I hit a weird bug where identical prices wouldn't match. Two orders at $100.50 would sometimes not cross:

```cpp
double a = 100.50;
double b = 100.20 + 100.30;  // Should be 100.50
// a == b → FALSE (in some cases)
```

This is the classic IEEE 754 floating point problem. Real exchanges don't use floats for this exact reason.

**Fix:** I switched everything to integer "ticks." $100.50 becomes `10050`. Integer comparison is always exact.

**Lesson 2: Never use floating point for money. I thought I knew this but had to learn it the hard way.**

---

## The humbling rewrite: actually measuring performance

When I put the real matching logic under a proper timer, the numbers were... not great. About 800K orders/sec. Not terrible, but not the "millions of TPS" I had been claiming.

I ran a profiler (well, I added a bunch of `chrono` timing). The bottleneck was clear: `std::map`. Every time I inserted or looked up a price level, it was doing O(log n) tree traversal with pointer chasing.

I read Ulrich Drepper's paper on memory and realized the problem: every `std::map` node is heap-allocated at a random address. The CPU cache can't predict where the next node is, so every access is a cache miss (~100ns penalty).

**Fix:** I replaced `std::map` with a flat `std::vector` — one entry per possible price tick. Price 10050 goes into `array[10050 - min_tick]`. O(1) access, and the entries are contiguous in memory so the cache prefetcher pulls in adjacent levels for free.

This alone got me from 800K to 1.3M orders/sec. A 1.6x improvement from just changing the data structure for cache friendliness — the algorithm (addOrder + match) was basically the same.

**Lesson 3: Cache effects matter more than algorithmic complexity for small n.**

---

## The cancel problem: O(n) was killing me

In real markets, a huge percentage of orders get cancelled. Market makers send an order, the price moves, they cancel it and send a new one. This happens constantly.

With `std::deque` as my order queue, cancelling meant scanning the entire queue to find the order:

```cpp
for (auto it = queue.begin(); it != queue.end(); ++it) {
    if (*it == order_to_cancel) {
        queue.erase(it);
        break;
    }
}
```

O(n) per cancel. With thousands of orders per price level, this was brutal.

**Fix:** Intrusive doubly-linked lists. Instead of putting orders INTO a container, each order carries its own `prev` and `next` pointers:

```cpp
struct Order {
    // ... data fields ...
    Order* prev = nullptr;
    Order* next = nullptr;
};
```

To cancel, you just relink the neighbors:
```cpp
order->prev->next = order->next;
order->next->prev = order->prev;
```

Three pointer operations. O(1). This was the single biggest performance win in the project.

**Lesson 4: Sometimes adding 16 bytes to a struct saves you from O(n) operations. The tradeoff is always worth thinking about.**

---

## The matching hot path: death by allocation

I profiled again (more `chrono` timestamps) and found another bottleneck: the matching function was returning `std::vector<Trade>`. Every call to `match()` was:

1. Construct a vector (malloc internally)
2. Push trades into it (possibly realloc)  
3. Return it (move semantics help, but still)

Over 2 million orders, that's 2 million vector constructions. Each one hits the heap allocator.

**Fix:** Callback templates. Instead of returning a vector, the caller passes a function:

```cpp
book.submitOrder(order, [](const Trade& t) {
    // do whatever you want with the trade
});
```

For benchmarking, you pass `[](const Trade&) {}` — a no-op. The compiler inlines it and removes it entirely. Zero cost.

**Lesson 5: On the hot path, every allocation counts. Callbacks beat return values when you need maximum throughput.**

---

## The result

After all three changes (flat array + intrusive lists + callback matching), the benchmark looked like this:

| Architecture | Throughput | p99 |
|---|---|---|
| Tree + Deque (original) | 861K/s | 3000ns |
| Array + Deque (intermediate) | 1.36M/s | 2200ns |
| Array + Intrusive (final) | 2.57M/s | 900ns |

3x faster. But the tail latency improvement was even more dramatic — p999 went from 45μs to 3.4μs. That's 13x better worst-case behavior.

The most satisfying part was that the improvements were cumulative and each one had a clear explanation:
- Flat array: cache locality
- Intrusive list: O(1) cancel
- Callback: zero allocation

---

## What I'd do differently next time

1. **Write the benchmark first.** If I had a proper benchmark from day one, I wouldn't have wasted time with the fake stress test.
2. **Read about real exchange architectures earlier.** The LMAX Disruptor paper and the Drepper memory paper should have been my starting points.
3. **Add proper observability.** I still don't have nanosecond-precision logging. Adding timing around every operation would have helped me find bottlenecks faster.
4. **Don't put impressive-sounding numbers on LinkedIn before verifying them.** "2.5 billion TPS" from an empty for-loop is not a metric anyone should brag about.

---

## What I actually learned

This project taught me more about systems programming than any course or textbook. Not because the code is particularly complex — it's ~1000 lines of C++17. But because every optimization forced me to understand a layer deeper:

- Why flat arrays beat trees → led me to understand CPU cache architecture
- Why intrusive lists beat containers → led me to understand memory layout
- Why callbacks beat return values → led me to understand compiler inlining

If you're a student looking to learn systems programming, I'd recommend building something like this. Not because the world needs another matching engine, but because the process of making it fast teaches you things you can't learn any other way.

[GitHub repo](https://github.com/saksham10arora-dotcom/Simple-HFT-Engine)
