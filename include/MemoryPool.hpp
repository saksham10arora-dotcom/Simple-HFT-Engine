#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <cstddef>
#include <cstdlib>
#include <vector>

namespace hft {

// High-performance arena allocator with free-list recycling.
// Uses raw storage — no default-construction of objects at init time.
// Allocations and deallocations are O(1).
template <typename T>
class MemoryPool
{
private:
    char*           storage_;        // Raw memory slab (no construction)
    std::vector<T*> free_list_;      // Stack of recycled slots
    size_t          capacity_;       // Total number of T slots
    size_t          next_;           // Bump-pointer index

public:
    explicit MemoryPool(size_t cap)
        : capacity_(cap), next_(0)
    {
        storage_ = static_cast<char*>(std::malloc(sizeof(T) * cap));
        free_list_.reserve(cap / 4);
    }

    ~MemoryPool() { std::free(storage_); }

    // Non-copyable, non-movable (owns raw memory)
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    // O(1) allocation: free-list first, bump-pointer fallback.
    // Returns nullptr if pool is completely exhausted.
    T* allocate()
    {
        if (!free_list_.empty())
        {
            T* ptr = free_list_.back();
            free_list_.pop_back();
            return ptr;
        }
        if (next_ < capacity_)
            return reinterpret_cast<T*>(storage_ + sizeof(T) * next_++);
        return nullptr;
    }

    // O(1) deallocation: returns slot to free-list for reuse.
    void deallocate(T* ptr) { free_list_.push_back(ptr); }

    // Reset the pool entirely (e.g. new trading day)
    void reset()
    {
        next_ = 0;
        free_list_.clear();
    }

    // Diagnostics
    size_t allocated_count() const { return next_ - free_list_.size(); }
    size_t capacity()        const { return capacity_; }
};

} // namespace hft

#endif