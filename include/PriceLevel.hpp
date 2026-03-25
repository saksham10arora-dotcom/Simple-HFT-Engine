#ifndef PRICELEVEL_HPP
#define PRICELEVEL_HPP

#include "Order.hpp"
#include <cstdint>

namespace hft {

// Intrusive doubly-linked list of orders at a single price level.
// All operations are O(1). Zero heap allocations.
struct PriceLevel
{
    Order*   head  = nullptr;
    Order*   tail  = nullptr;
    uint32_t count = 0;

    bool empty() const { return head == nullptr; }

    // O(1) append to the back (FIFO: oldest at head, newest at tail)
    void push_back(Order* o)
    {
        o->prev = tail;
        o->next = nullptr;
        if (tail) tail->next = o;
        else      head = o;
        tail = o;
        ++count;
    }

    // O(1) remove any order given its pointer (intrusive list advantage)
    void remove(Order* o)
    {
        if (o->prev) o->prev->next = o->next;
        else         head = o->next;

        if (o->next) o->next->prev = o->prev;
        else         tail = o->prev;

        o->prev = o->next = nullptr;
        --count;
    }

    // O(1) access to the oldest order (FIFO front)
    Order* front() const { return head; }

    // O(1) remove the oldest order
    void pop_front()
    {
        if (!head) return;
        Order* old = head;
        head = head->next;
        if (head) head->prev = nullptr;
        else      tail = nullptr;
        old->prev = old->next = nullptr;
        --count;
    }
};

} // namespace hft

#endif
