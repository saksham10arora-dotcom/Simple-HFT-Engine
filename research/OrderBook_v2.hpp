#ifndef ORDERBOOK_V2_HPP
#define ORDERBOOK_V2_HPP

// ════════════════════════════════════════════════════════════════
//  Architecture A: Tree-Based LOB (v2.0 — Reconstructed)
//
//  Bids: std::map<tick, std::deque<Order*>> (descending)
//  Asks: std::map<tick, std::deque<Order*>> (ascending)
//  Cancel: O(n) linear scan in deque
//  Insert: O(log n) tree insertion
//  Match:  O(log n) tree traversal
// ════════════════════════════════════════════════════════════════

#include <map>
#include <deque>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>

namespace arch_a {

enum class Side : uint8_t { Buy, Sell };

// 24-byte POD struct — no intrusive list overhead
struct Order
{
    uint64_t id;
    int64_t  price_ticks;
    uint32_t quantity;
    Side     side;
};

struct Trade
{
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    int64_t  price_ticks;
    uint32_t quantity;
};

// Simple pool (matches original v2 design)
class MemoryPool
{
    std::vector<Order> pool_;
    std::vector<Order*> free_list_;
    size_t next_ = 0;
public:
    explicit MemoryPool(size_t cap) : pool_(cap) { free_list_.reserve(cap / 4); }

    Order* allocate()
    {
        if (!free_list_.empty()) { auto* p = free_list_.back(); free_list_.pop_back(); return p; }
        if (next_ < pool_.size()) return &pool_[next_++];
        return nullptr;
    }
    void deallocate(Order* p) { free_list_.push_back(p); }
    void reset() { next_ = 0; free_list_.clear(); }
};

class OrderBook
{
    std::map<int64_t, std::deque<Order*>, std::greater<int64_t>> bids_;
    std::map<int64_t, std::deque<Order*>> asks_;
    std::unordered_map<uint64_t, Order*> lookup_;
    uint64_t next_trade_id_ = 1;

public:
    void addOrder(Order* o)
    {
        lookup_[o->id] = o;
        if (o->side == Side::Buy) bids_[o->price_ticks].push_back(o);
        else                      asks_[o->price_ticks].push_back(o);
    }

    bool cancelOrder(uint64_t id)
    {
        auto it = lookup_.find(id);
        if (it == lookup_.end()) return false;
        Order* o = it->second;
        lookup_.erase(it);
        if (o->side == Side::Buy) removeFrom(bids_, o);
        else                      removeFrom(asks_, o);
        return true;
    }

    // Match + return vector (original v2 design — allocates on hot path)
    std::vector<Trade> match()
    {
        std::vector<Trade> trades;
        while (!bids_.empty() && !asks_.empty())
        {
            auto bi = bids_.begin();
            auto ai = asks_.begin();
            if (bi->first < ai->first) break;

            Order* buy  = bi->second.front();
            Order* sell = ai->second.front();
            uint32_t fill = std::min(buy->quantity, sell->quantity);

            trades.push_back({next_trade_id_++, buy->id, sell->id, ai->first, fill});
            buy->quantity  -= fill;
            sell->quantity -= fill;

            if (buy->quantity == 0)  { bi->second.pop_front(); lookup_.erase(buy->id);  if (bi->second.empty()) bids_.erase(bi); }
            if (sell->quantity == 0) { ai->second.pop_front(); lookup_.erase(sell->id); if (ai->second.empty()) asks_.erase(ai); }
        }
        return trades;
    }

    size_t totalOrders() const { return lookup_.size(); }

private:
    template <typename M>
    void removeFrom(M& book, Order* o)
    {
        auto it = book.find(o->price_ticks);
        if (it == book.end()) return;
        auto& dq = it->second;
        dq.erase(std::remove(dq.begin(), dq.end(), o), dq.end());
        if (dq.empty()) book.erase(it);
    }
};

} // namespace arch_a

#endif
