#ifndef ORDER_HPP
#define ORDER_HPP

#include <cstdint>

// We use an 'enum class' for type safety and speed
enum class Side
{
    Buy,
    Sell
};

// POD (Plain Old Data) struct: This is the most cache-efficient way
// to store data in C++. It tells the CPU exactly how to align memory.
struct Order
{
    uint64_t id;       // Unique ID for the order
    double price;      // Price at which the user wants to trade
    uint32_t quantity; // Number of units
    Side side;         // Buy or Sell
};

#endif