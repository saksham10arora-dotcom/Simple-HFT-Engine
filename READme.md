# Ultra-Low Latency HFT Matching Engine v3.0

A high-performance C++ Limit Order Book (LOB) designed for High-Frequency Trading. Features deterministic matching with **O(1) flat-array price levels**, **intrusive doubly-linked lists** for zero-overhead order management, and **zero-allocation matching** via callback templates.

## 🚀 Architecture

```
                   ┌─────────────────────────────────────┐
                   │         OrderBook (v3.0)             │
                   │                                      │
  submitOrder() ──▶│  Flat Array of PriceLevel structs    │
                   │  [9000] [9001] ... [10000] ... [11000]│
                   │     │                  │              │
                   │     ▼                  ▼              │
                   │  PriceLevel         PriceLevel        │
                   │  ┌─────┐            ┌─────┐          │
                   │  │ O←→O│            │O←→O←→O│        │
                   │  └─────┘            └───────┘        │
                   │  (intrusive         (intrusive        │
                   │   linked list)       linked list)     │
                   │                                      │
                   │  best_bid_ ──────▶ O(1) BBO          │
                   │  best_ask_ ──────▶ O(1) BBO          │
                   │                                      │
  on_trade(T) ◀───│  Zero-alloc callback template        │
                   └─────────────────────────────────────┘
                              ▲
                              │
                   ┌──────────┴──────────┐
                   │  MemoryPool<Order>   │
                   │  Raw storage arena   │
                   │  + free-list recycle │
                   └─────────────────────┘
```

## ✨ Features

### v3.0 Performance Core
- **Flat-array price levels** — O(1) insert/lookup by tick, replaces `std::map` red-black tree
- **Intrusive doubly-linked lists** — O(1) cancel/remove of any order, replaces `std::deque`
- **Zero-allocation matching** — Callback template (`on_trade` lambda), no `std::vector` on hot path
- **Best bid/ask tracking** — O(1) BBO queries with lazy scan updates
- **Raw-storage memory pool** — Arena allocator, no default-construction overhead

### Exchange Features
- **Integer-tick pricing** — `int64_t` ticks eliminate floating-point comparison bugs
- **Price-time priority** — FIFO ordering via intrusive lists at each price level
- **Order types** — Limit, Market, IOC (Immediate-or-Cancel), FOK (Fill-or-Kill)
- **Order lifecycle** — `submitOrder`, `cancelOrder`, `modifyOrder` with O(1) ID lookup
- **Trade execution reports** — `Trade` struct with trade ID, counterparty IDs, price, quantity

### Quality
- **21 unit tests** — Pool, matching, partial fills, FIFO, cancel, modify, Market/IOC/FOK, BBO tracking
- **2M-order benchmark** — Throughput (orders/sec), latency percentiles (p50/p90/p99/p999)
- **No external dependencies** — Pure C++17 standard library
- **Cross-platform** — MSVC, GCC, Clang

## 📂 Project Structure

```
HFT_Engine/
├── include/
│   ├── Order.hpp        # Order struct with intrusive list pointers, OrderType enum
│   ├── Trade.hpp        # Execution report struct
│   ├── PriceLevel.hpp   # Intrusive doubly-linked list at a price level
│   ├── MemoryPool.hpp   # Raw-storage arena allocator + free-list
│   └── OrderBook.hpp    # Flat-array LOB, matching, submit/cancel/modify
├── src/
│   └── main.cpp         # Engine demo: Limit, Market, IOC orders + benchmarks
├── tests/
│   ├── test_engine.cpp  # 21 unit tests
│   └── benchmark.cpp    # 2M-order throughput & latency harness
└── CMakeLists.txt
```

## 🛠️ Build & Run

```bash
cd build
cmake ..
cmake --build . --config Release
```

### Run the engine demo
```bash
Release\hft_engine.exe
```

### Run unit tests (21 tests)
```bash
Release\test_engine.exe
```

### Run performance benchmark (2M orders)
```bash
Release\benchmark.exe
```

## 📊 Performance

Run `benchmark.exe` yourself to get numbers specific to your hardware. The benchmark processes **2 million orders** using the zero-allocation callback path and reports:

| Metric | Description |
|--------|-------------|
| Allocation latency | ns per order (arena pool allocator) |
| Throughput | Orders processed per second (submit + match) |
| p50 / p90 / p99 / p999 | Submit+match latency percentiles |
| Deallocation latency | ns per order (free-list return) |

### v2 → v3 Changes

| Component | v2.0 | v3.0 |
|-----------|------|------|
| Price levels | `std::map` (RB-tree, O(log n)) | Flat array (O(1)) |
| Order queue | `std::deque` (O(n) cancel) | Intrusive linked list (O(1) cancel) |
| Matching | Returns `std::vector<Trade>` (heap alloc) | Callback template (zero alloc) |
| Memory pool | `std::vector<T>` (default-constructs all) | Raw `char*` storage |
| Order types | Limit only | Limit, Market, IOC, FOK |
| Unit tests | 12 | 21 |

## 🔧 Technical Details

- **C++17** — `constexpr`, structured bindings, `enum class` with backing type
- **Cache-line-aware** — `Order` struct ≤ 64 bytes (`static_assert` enforced)
- **Zero-copy hot path** — Pointer-passing through pre-allocated arena buffers
- **Deterministic matching** — Price-time priority with correct residual tracking
- **Configurable tick range** — `OrderBook(min_tick, max_tick)` for any price range

## 📄 API

```cpp
using namespace hft;

MemoryPool<Order> pool(1'000'000);
OrderBook book(9000, 11000);  // Tick range: $90.00 - $110.00

// Allocate and submit a limit order
Order* buy = pool.allocate();
*buy = {/*id=*/1, /*price=*/10000, /*qty=*/50, Side::Buy};

size_t trades = book.submitOrder(buy, [](const Trade& t) {
    // Zero-allocation callback — called for each fill
    std::cout << "Filled " << t.quantity << " @ " << t.price_ticks << "\n";
});

// Submit a market order
Order* mkt = pool.allocate();
*mkt = {2, 0, 100, Side::Buy, OrderType::Market};
book.submitOrder(mkt, [](const Trade&) {});

// Cancel / Modify
book.cancelOrder(1);        // O(1) via intrusive list
book.modifyOrder(2, 50);    // Reduce quantity (keeps time priority)
```