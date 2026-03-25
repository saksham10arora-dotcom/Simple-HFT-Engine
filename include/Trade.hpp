#ifndef TRADE_HPP
#define TRADE_HPP

#include <cstdint>

namespace hft {

// Immutable record of a matched trade execution.
struct Trade
{
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t  price_ticks;   // Execution price
    uint32_t quantity;      // Filled quantity
};

} // namespace hft

#endif
