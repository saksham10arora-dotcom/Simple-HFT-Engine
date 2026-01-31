#include "../include/MemoryPool.hpp"
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"
#include <iostream>
#include <thread> // For a small delay

int main()
{
    MemoryPool<Order> pool(1000);
    OrderBook book;

    std::cout << "--- HFT ENGINE LIVE FEED ---" << std::endl;

    for (int i = 0; i < 50; ++i)
    {
        Order *o = pool.allocate();
        o->id = i;
        o->quantity = 10;

        // Simulate a fluctuating market
        o->side = (i % 3 == 0) ? Side::Sell : Side::Buy;
        o->price = 100.0 + (rand() % 10);

        book.addOrder(o);
        book.match();

        // Print the top of the book every iteration
        book.printBBO();
    }

    return 0;
}