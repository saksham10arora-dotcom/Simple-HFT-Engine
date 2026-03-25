#ifndef ORDERBOOK_V2B_HPP
#define ORDERBOOK_V2B_HPP

// ════════════════════════════════════════════════════════════════
//  Architecture B: Flat Array + Deque (Intermediate Design)
//
//  Bids: vector<deque<Order*>> indexed by tick (O(1) access)
//  Asks: vector<deque<Order*>> indexed by tick (O(1) access)
//  Cancel: O(n) linear scan in deque (same as v2)
//  Insert: O(1) direct index
//  Match:  O(1) best bid/ask tracking
//
//  This isolates the benefit of flat-array vs tree,
//  without the intrusive list upgrade.
// ════════════════════════════════════════════════════════════════

#include <deque>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>

namespace arch_b {

enum class Side : uint8_t { Buy, Sell };

// Same 24-byte Order as Architecture A (no intrusive pointers)
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
    std::vector<std::deque<Order*>> bid_levels_;
    std::vector<std::deque<Order*>> ask_levels_;
    std::unordered_map<uint64_t, Order*> lookup_;

    int64_t min_tick_, max_tick_;
    int64_t best_bid_, best_ask_;
    uint64_t next_trade_id_ = 1;

    size_t idx(int64_t tick) const { return static_cast<size_t>(tick - min_tick_); }

    void updateBestBid()
    {
        while (best_bid_ >= min_tick_ && bid_levels_[idx(best_bid_)].empty())
            --best_bid_;
    }
    void updateBestAsk()
    {
        while (best_ask_ <= max_tick_ && ask_levels_[idx(best_ask_)].empty())
            ++best_ask_;
    }

public:
    explicit OrderBook(int64_t min_tick = 0, int64_t max_tick = 20000)
        : min_tick_(min_tick), max_tick_(max_tick)
        , best_bid_(min_tick - 1), best_ask_(max_tick + 1)
    {
        size_t n = static_cast<size_t>(max_tick - min_tick + 1);
        bid_levels_.resize(n);
        ask_levels_.resize(n);
    }

    void addOrder(Order* o)
    {
        lookup_[o->id] = o;
        if (o->side == Side::Buy)
        {
            bid_levels_[idx(o->price_ticks)].push_back(o);
            if (o->price_ticks > best_bid_) best_bid_ = o->price_ticks;
        }
        else
        {
            ask_levels_[idx(o->price_ticks)].push_back(o);
            if (o->price_ticks < best_ask_) best_ask_ = o->price_ticks;
        }
    }

    bool cancelOrder(uint64_t id)
    {
        auto it = lookup_.find(id);
        if (it == lookup_.end()) return false;
        Order* o = it->second;
        lookup_.erase(it);

        if (o->side == Side::Buy)
        {
            auto& dq = bid_levels_[idx(o->price_ticks)];
            dq.erase(std::remove(dq.begin(), dq.end(), o), dq.end());
            if (dq.empty() && o->price_ticks == best_bid_) updateBestBid();
        }
        else
        {
            auto& dq = ask_levels_[idx(o->price_ticks)];
            dq.erase(std::remove(dq.begin(), dq.end(), o), dq.end());
            if (dq.empty() && o->price_ticks == best_ask_) updateBestAsk();
        }
        return true;
    }

    // Match: same vector-returning pattern as v2 (for fair comparison)
    std::vector<Trade> match()
    {
        std::vector<Trade> trades;

        while (best_bid_ >= min_tick_ && best_ask_ <= max_tick_ && best_bid_ >= best_ask_)
        {
            auto& bl = bid_levels_[idx(best_bid_)];
            auto& al = ask_levels_[idx(best_ask_)];
            if (bl.empty() || al.empty()) break;

            Order* buy  = bl.front();
            Order* sell = al.front();
            uint32_t fill = std::min(buy->quantity, sell->quantity);

            trades.push_back({next_trade_id_++, buy->id, sell->id, best_ask_, fill});
            buy->quantity  -= fill;
            sell->quantity -= fill;

            if (buy->quantity == 0)  { bl.pop_front(); lookup_.erase(buy->id);  if (bl.empty()) updateBestBid(); }
            if (sell->quantity == 0) { al.pop_front(); lookup_.erase(sell->id); if (al.empty()) updateBestAsk(); }
        }
        return trades;
    }

    size_t totalOrders() const { return lookup_.size(); }
};

} // namespace arch_b

#endif
