#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "Order.hpp"
#include "Trade.hpp"
#include "PriceLevel.hpp"

#include <vector>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <cstdint>

namespace hft {

// ════════════════════════════════════════════════════════════════
//  High-performance Limit Order Book
//
//  v3.0 Architecture:
//    • Two flat arrays (bids + asks) — O(1) insert/lookup by tick
//    • Intrusive doubly-linked lists — O(1) cancel
//    • Zero-allocation matching via callback template
//    • Supports: Limit, Market, IOC, FOK order types
//    • Tracks best bid/ask for O(1) BBO queries
// ════════════════════════════════════════════════════════════════
class OrderBook
{
private:
    std::vector<PriceLevel> bid_levels_;    // Flat array for buys
    std::vector<PriceLevel> ask_levels_;    // Flat array for sells
    std::unordered_map<uint64_t, Order*> order_lookup_;

    int64_t min_tick_;
    int64_t max_tick_;

    int64_t best_bid_;       // Current best bid (highest buy price)
    int64_t best_ask_;       // Current best ask (lowest sell price)

    uint64_t next_trade_id_ = 1;

    uint32_t bid_level_count_ = 0;
    uint32_t ask_level_count_ = 0;
    size_t   total_orders_    = 0;

    // ── Helpers ─────────────────────────────────────────────────
    size_t index(int64_t tick) const
    {
        return static_cast<size_t>(tick - min_tick_);
    }

    PriceLevel& bidLevel(int64_t tick) { return bid_levels_[index(tick)]; }
    PriceLevel& askLevel(int64_t tick) { return ask_levels_[index(tick)]; }
    const PriceLevel& bidLevel(int64_t tick) const { return bid_levels_[index(tick)]; }
    const PriceLevel& askLevel(int64_t tick) const { return ask_levels_[index(tick)]; }

    void updateBestBid()
    {
        while (best_bid_ >= min_tick_ && bidLevel(best_bid_).empty())
            --best_bid_;
    }

    void updateBestAsk()
    {
        while (best_ask_ <= max_tick_ && askLevel(best_ask_).empty())
            ++best_ask_;
    }

    // Insert order into the correct side's price level
    void insertIntoBook(Order* order)
    {
        if (order->side == Side::Buy)
        {
            auto& level = bidLevel(order->price_ticks);
            bool was_empty = level.empty();
            level.push_back(order);
            if (was_empty) ++bid_level_count_;
            if (order->price_ticks > best_bid_)
                best_bid_ = order->price_ticks;
        }
        else
        {
            auto& level = askLevel(order->price_ticks);
            bool was_empty = level.empty();
            level.push_back(order);
            if (was_empty) ++ask_level_count_;
            if (order->price_ticks < best_ask_)
                best_ask_ = order->price_ticks;
        }
        ++total_orders_;
    }

    // Remove order from book (for cancel)
    void removeFromBook(Order* order)
    {
        if (order->side == Side::Buy)
        {
            auto& level = bidLevel(order->price_ticks);
            level.remove(order);
            --total_orders_;
            if (level.empty())
            {
                --bid_level_count_;
                if (order->price_ticks == best_bid_) updateBestBid();
            }
        }
        else
        {
            auto& level = askLevel(order->price_ticks);
            level.remove(order);
            --total_orders_;
            if (level.empty())
            {
                --ask_level_count_;
                if (order->price_ticks == best_ask_) updateBestAsk();
            }
        }
    }

    // Remove a fully filled passive order (always the front of its level)
    void removeFilledBid(Order* order, PriceLevel& level)
    {
        level.pop_front();
        order_lookup_.erase(order->id);
        --total_orders_;
        if (level.empty())
        {
            --bid_level_count_;
            if (order->price_ticks == best_bid_) updateBestBid();
        }
    }

    void removeFilledAsk(Order* order, PriceLevel& level)
    {
        level.pop_front();
        order_lookup_.erase(order->id);
        --total_orders_;
        if (level.empty())
        {
            --ask_level_count_;
            if (order->price_ticks == best_ask_) updateBestAsk();
        }
    }

    // Check if a FOK order can be fully filled
    bool canFillFully(const Order* order) const
    {
        uint32_t remaining = order->quantity;

        if (order->side == Side::Buy)
        {
            for (int64_t t = best_ask_; t <= max_tick_ && remaining > 0; ++t)
            {
                if (order->type == OrderType::Limit && order->price_ticks < t)
                    break;
                const auto& level = askLevel(t);
                Order* cur = level.head;
                while (cur && remaining > 0)
                {
                    uint32_t fill = std::min(remaining, cur->quantity);
                    remaining -= fill;
                    cur = cur->next;
                }
            }
        }
        else
        {
            for (int64_t t = best_bid_; t >= min_tick_ && remaining > 0; --t)
            {
                if (order->type == OrderType::Limit && order->price_ticks > t)
                    break;
                const auto& level = bidLevel(t);
                Order* cur = level.head;
                while (cur && remaining > 0)
                {
                    uint32_t fill = std::min(remaining, cur->quantity);
                    remaining -= fill;
                    cur = cur->next;
                }
            }
        }
        return remaining == 0;
    }

    // ── Aggressive matching ─────────────────────────────────────
    template <typename Callback>
    size_t matchBuyAggressor(Order* buy, Callback&& on_trade)
    {
        size_t trades = 0;

        while (buy->quantity > 0 && best_ask_ <= max_tick_)
        {
            if (buy->type == OrderType::Limit && buy->price_ticks < best_ask_)
                break;

            auto& level = askLevel(best_ask_);
            while (buy->quantity > 0 && !level.empty())
            {
                Order* sell = level.front();
                uint32_t fill = std::min(buy->quantity, sell->quantity);

                on_trade(Trade{next_trade_id_++, buy->id, sell->id, best_ask_, fill});
                ++trades;

                buy->quantity  -= fill;
                sell->quantity -= fill;

                if (sell->quantity == 0)
                    removeFilledAsk(sell, level);
            }

            if (level.empty()) updateBestAsk();
        }
        return trades;
    }

    template <typename Callback>
    size_t matchSellAggressor(Order* sell, Callback&& on_trade)
    {
        size_t trades = 0;

        while (sell->quantity > 0 && best_bid_ >= min_tick_)
        {
            if (sell->type == OrderType::Limit && sell->price_ticks > best_bid_)
                break;

            auto& level = bidLevel(best_bid_);
            while (sell->quantity > 0 && !level.empty())
            {
                Order* buy = level.front();
                uint32_t fill = std::min(sell->quantity, buy->quantity);

                on_trade(Trade{next_trade_id_++, buy->id, sell->id, best_bid_, fill});
                ++trades;

                sell->quantity -= fill;
                buy->quantity  -= fill;

                if (buy->quantity == 0)
                    removeFilledBid(buy, level);
            }

            if (level.empty()) updateBestBid();
        }
        return trades;
    }

public:
    // ── Constructor ─────────────────────────────────────────────
    explicit OrderBook(int64_t min_tick = 0, int64_t max_tick = 20000)
        : min_tick_(min_tick)
        , max_tick_(max_tick)
        , best_bid_(min_tick - 1)
        , best_ask_(max_tick + 1)
    {
        size_t num_levels = static_cast<size_t>(max_tick - min_tick + 1);
        bid_levels_.resize(num_levels);
        ask_levels_.resize(num_levels);
    }

    // ════════════════════════════════════════════════════════════
    //  submitOrder — Main entry point (realistic exchange model)
    // ════════════════════════════════════════════════════════════
    template <typename Callback>
    size_t submitOrder(Order* order, Callback&& on_trade)
    {
        if (order->type == OrderType::FOK)
        {
            if (!canFillFully(order))
                return 0;
        }

        size_t trades = (order->side == Side::Buy)
            ? matchBuyAggressor(order, on_trade)
            : matchSellAggressor(order, on_trade);

        if (order->quantity > 0)
        {
            if (order->type == OrderType::Limit)
            {
                order_lookup_[order->id] = order;
                insertIntoBook(order);
            }
        }

        return trades;
    }

    size_t submitOrder(Order* order, std::vector<Trade>& out)
    {
        return submitOrder(order, [&out](const Trade& t) { out.push_back(t); });
    }

    // ── addOrder (test helper) ──────────────────────────────────
    void addOrder(Order* order)
    {
        order_lookup_[order->id] = order;
        insertIntoBook(order);
    }

    // ── match (batch) ───────────────────────────────────────────
    template <typename Callback>
    size_t match(Callback&& on_trade)
    {
        size_t trades = 0;

        while (best_bid_ >= min_tick_ && best_ask_ <= max_tick_ && best_bid_ >= best_ask_)
        {
            auto& bid_level = bidLevel(best_bid_);
            auto& ask_level = askLevel(best_ask_);

            if (bid_level.empty() || ask_level.empty()) break;

            Order* buy  = bid_level.front();
            Order* sell = ask_level.front();

            uint32_t fill = std::min(buy->quantity, sell->quantity);
            int64_t price = sell->price_ticks;

            on_trade(Trade{next_trade_id_++, buy->id, sell->id, price, fill});
            ++trades;

            buy->quantity  -= fill;
            sell->quantity -= fill;

            if (buy->quantity == 0)  removeFilledBid(buy, bid_level);
            if (sell->quantity == 0) removeFilledAsk(sell, ask_level);
        }

        return trades;
    }

    std::vector<Trade> match()
    {
        std::vector<Trade> trades;
        match([&trades](const Trade& t) { trades.push_back(t); });
        return trades;
    }

    // ── Cancel ──────────────────────────────────────────────────
    bool cancelOrder(uint64_t id)
    {
        auto it = order_lookup_.find(id);
        if (it == order_lookup_.end()) return false;

        Order* order = it->second;
        order_lookup_.erase(it);
        removeFromBook(order);
        return true;
    }

    // ── Modify ──────────────────────────────────────────────────
    bool modifyOrder(uint64_t id, uint32_t new_qty)
    {
        auto it = order_lookup_.find(id);
        if (it == order_lookup_.end()) return false;

        if (new_qty == 0) return cancelOrder(id);

        it->second->quantity = new_qty;
        return true;
    }

    // ── Market Data ─────────────────────────────────────────────
    int64_t bestBid() const { return best_bid_; }
    int64_t bestAsk() const { return best_ask_; }

    void printBBO() const
    {
        bool has_bid = (best_bid_ >= min_tick_);
        bool has_ask = (best_ask_ <= max_tick_);

        if (has_bid && has_ask)
        {
            double bid = static_cast<double>(best_bid_) / TICK_SIZE;
            double ask = static_cast<double>(best_ask_) / TICK_SIZE;
            std::cout << "Best Bid: $" << bid
                      << " | Best Ask: $" << ask
                      << " | Spread: $" << (ask - bid) << "\n";
        }
        else if (has_bid)
            std::cout << "Best Bid: $" << static_cast<double>(best_bid_) / TICK_SIZE
                      << " | No asks\n";
        else if (has_ask)
            std::cout << "No bids | Best Ask: $" << static_cast<double>(best_ask_) / TICK_SIZE << "\n";
        else
            std::cout << "Book empty\n";
    }

    // ── Diagnostics ─────────────────────────────────────────────
    size_t totalOrders()   const { return total_orders_; }
    size_t bidLevelCount() const { return bid_level_count_; }
    size_t askLevelCount() const { return ask_level_count_; }
    int64_t minTick()      const { return min_tick_; }
    int64_t maxTick()      const { return max_tick_; }
};

} // namespace hft

#endif