#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <vector>
#include <iostream>

// We use a Template so this pool can store Orders or any other object
template <typename T>
class MemoryPool
{
private:
    std::vector<T> pool;   // The actual block of memory
    size_t next_available; // Index of the next free object

public:
    // Constructor: Pre-allocates the memory 'slab'
    explicit MemoryPool(size_t size) : pool(size), next_available(0)
    {
        std::cout << "HFT Memory Pool: " << size << " objects pre-allocated at startup.\n";
    }

    // High-speed allocation: Just returns a pointer to the next block
    // No System Calls, no OS overhead.
    T *allocate()
    {
        if (next_available < pool.size())
        {
            return &pool[next_available++];
        }
        std::cerr << "Error: Memory Pool Exhausted!\n";
        return nullptr;
    }

    // Reset the pool (for a new trading day)
    void reset()
    {
        next_available = 0;
    }
};

#endif