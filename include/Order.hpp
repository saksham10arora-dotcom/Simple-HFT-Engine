#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>

namespace hft {

// Price is stored as integer ticks to avoid floating-point precision issues.
// Example: TICK_SIZE=100 means 1 tick = $0.01, so $100.50 = 10050 ticks.
static constexpr int64_t TICK_SIZE = 100;

// Type-safe side enum
enum class Side : uint8_t { Buy, Sell };

// Order type: determines fill-or-rest behavior
enum class OrderType : uint8_t
{
    Limit,   // Rest in book if not fully filled
    Market,  // Fill at best available, cancel remainder
    IOC,     // Immediate-or-Cancel: fill what crosses, cancel rest
    FOK      // Fill-or-Kill: fill entirely or reject entirely
};

// Cache-line-friendly order with intrusive list pointers for O(1) removal.
// Layout: hot fields first (price, qty, side), then list pointers.
struct Order
{
    uint64_t  id;                       // Unique order ID
    int64_t   price_ticks;              // Price in integer ticks
    uint32_t  quantity;                 // Remaining quantity (updated on partial fills)
    Side      side;                     // Buy or Sell
    OrderType type = OrderType::Limit;  // Order type (default: Limit)

    // Intrusive doubly-linked list pointers — O(1) insert/remove
    Order* prev = nullptr;
    Order* next = nullptr;
};

// Ensure Order fits in a single cache line (64 bytes)
static_assert(sizeof(Order) <= 64, "Order struct exceeds cache line size");

} // namespace hft

#endif