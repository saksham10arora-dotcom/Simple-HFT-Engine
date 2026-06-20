#ifndef SPSC_RING_HPP
#define SPSC_RING_HPP

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>

namespace hft {

// ── Lock-free single-producer / single-consumer ring buffer ──────────────
//
// One thread pushes, one thread pops, no mutexes. Correctness rests on three
// things, each of which an interviewer will ask about:
//
//   1. Power-of-two capacity -> index with a bitwise mask instead of a modulo.
//   2. Acquire/release ordering (not seq_cst): the producer's release store on
//      head_ publishes the slot write that precedes it; the consumer's acquire
//      load pairs with it. No full barriers -> no needless contention.
//   3. head_ and tail_ live on separate cache lines (alignas(64)) so the two
//      threads never fight over the same line (false sharing).
//
// Single producer + single consumer means each index has exactly one writer,
// so there is no CAS loop and no ABA problem.
template <typename T>
class SPSCRing
{
public:
    explicit SPSCRing(size_t capacity_pow2)
        : capacity_(capacity_pow2),
          mask_(capacity_pow2 - 1),
          buffer_(capacity_pow2)
    {
        // Capacity must be a power of two for the mask trick to be valid.
        assert(capacity_pow2 >= 2 &&
               (capacity_pow2 & (capacity_pow2 - 1)) == 0 &&
               "SPSCRing capacity must be a power of two");
    }

    // Producer thread ONLY.
    bool try_push(const T& item)
    {
        const size_t head = head_.load(std::memory_order_relaxed);
        // Full when the buffer already holds `capacity_` unread items.
        if (head - tail_.load(std::memory_order_acquire) >= capacity_)
            return false;
        buffer_[head & mask_] = item;
        head_.store(head + 1, std::memory_order_release); // publish to consumer
        return true;
    }

    // Consumer thread ONLY.
    bool try_pop(T& out)
    {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return false; // empty
        out = buffer_[tail & mask_];
        tail_.store(tail + 1, std::memory_order_release); // free the slot
        return true;
    }

    size_t capacity() const { return capacity_; }

private:
    const size_t capacity_;
    const size_t mask_;
    std::vector<T> buffer_;

    // Separate cache lines: producer owns head_, consumer owns tail_.
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

} // namespace hft

#endif // SPSC_RING_HPP
