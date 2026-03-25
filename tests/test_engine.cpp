#include "../include/MemoryPool.hpp"
#include "../include/Order.hpp"
#include "../include/OrderBook.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

using namespace hft;

// ── Test Helpers ───────────────────────────────────────────────
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void name()
#define RUN(name)                                                     \
    do {                                                              \
        std::cout << "  Running " #name "... ";                       \
        try { name(); std::cout << "PASS\n"; ++tests_passed; }       \
        catch (const std::exception& e)                               \
        { std::cout << "FAIL (" << e.what() << ")\n"; ++tests_failed; } \
    } while(0)

static void check_eq(long long a, long long b, const char* expr, const char* file, int line)
{
    if (a != b)
    {
        std::string msg = std::string(file) + ":" + std::to_string(line)
                        + ": " + expr + " (" + std::to_string(a) + " != " + std::to_string(b) + ")";
        throw std::runtime_error(msg);
    }
}

#define ASSERT_EQ(a, b) check_eq(static_cast<long long>(a), static_cast<long long>(b), \
                                  #a " == " #b, __FILE__, __LINE__)
#define ASSERT_TRUE(x)  do { if (!(x)) throw std::runtime_error(#x " is false"); } while(0)
#define ASSERT_FALSE(x) do { if  (x)  throw std::runtime_error(#x " is true");  } while(0)

// ═══════════════════════════════════════════════════════════════
//  Pool Tests
// ═══════════════════════════════════════════════════════════════

TEST(test_pool_basic_alloc)
{
    MemoryPool<Order> pool(10);
    Order* o = pool.allocate();
    ASSERT_TRUE(o != nullptr);
    ASSERT_EQ(pool.allocated_count(), 1);
}

TEST(test_pool_exhaustion)
{
    MemoryPool<Order> pool(3);
    pool.allocate();
    pool.allocate();
    pool.allocate();
    Order* o = pool.allocate();
    ASSERT_TRUE(o == nullptr);
}

TEST(test_pool_dealloc_reuse)
{
    MemoryPool<Order> pool(2);
    Order* a = pool.allocate();
    Order* b = pool.allocate();
    (void)b;
    ASSERT_TRUE(pool.allocate() == nullptr);

    pool.deallocate(a);
    Order* c = pool.allocate();
    ASSERT_TRUE(c != nullptr);
    ASSERT_TRUE(c == a); // Should reuse the same memory
}

TEST(test_pool_reset)
{
    MemoryPool<Order> pool(5);
    for (int i = 0; i < 5; ++i) pool.allocate();
    ASSERT_TRUE(pool.allocate() == nullptr);

    pool.reset();
    ASSERT_TRUE(pool.allocate() != nullptr);
}

// ═══════════════════════════════════════════════════════════════
//  OrderBook Tests (using addOrder + match for test setup)
// ═══════════════════════════════════════════════════════════════

TEST(test_add_order)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* o = pool.allocate();
    *o = {1, 10000, 50, Side::Buy};
    book.addOrder(o);

    ASSERT_EQ(book.totalOrders(), 1);
    ASSERT_EQ(book.bidLevelCount(), 1);
    ASSERT_EQ(book.askLevelCount(), 0);
}

TEST(test_simple_match)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* buy  = pool.allocate(); *buy  = {1, 10000, 50, Side::Buy};
    Order* sell = pool.allocate(); *sell = {2, 10000, 50, Side::Sell};

    book.addOrder(buy);
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(trades[0].buy_order_id, 1);
    ASSERT_EQ(trades[0].sell_order_id, 2);
    ASSERT_EQ(book.totalOrders(), 0);
}

TEST(test_partial_fill)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* buy  = pool.allocate(); *buy  = {1, 10000, 100, Side::Buy};
    Order* sell = pool.allocate(); *sell = {2, 10000,  30, Side::Sell};

    book.addOrder(buy);
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 30);
    ASSERT_EQ(buy->quantity, 70);
    ASSERT_EQ(book.totalOrders(), 1);
    ASSERT_EQ(book.bidLevelCount(), 1);
}

TEST(test_fifo_ordering)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* buy1 = pool.allocate(); *buy1 = {1, 10000, 10, Side::Buy};
    Order* buy2 = pool.allocate(); *buy2 = {2, 10000, 10, Side::Buy};
    Order* sell = pool.allocate(); *sell = {3, 10000, 10, Side::Sell};

    book.addOrder(buy1);
    book.addOrder(buy2);
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].buy_order_id, 1); // buy1 filled first (FIFO)
    ASSERT_EQ(book.totalOrders(), 1);
}

TEST(test_no_match_when_spread)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* buy  = pool.allocate(); *buy  = {1, 9900, 50, Side::Buy};
    Order* sell = pool.allocate(); *sell = {2, 10100, 50, Side::Sell};

    book.addOrder(buy);
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 0);
    ASSERT_EQ(book.totalOrders(), 2);
}

TEST(test_cancel_order)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* o = pool.allocate(); *o = {42, 10000, 50, Side::Buy};
    book.addOrder(o);
    ASSERT_EQ(book.totalOrders(), 1);

    ASSERT_TRUE(book.cancelOrder(42));
    ASSERT_EQ(book.totalOrders(), 0);
    ASSERT_EQ(book.bidLevelCount(), 0);

    ASSERT_FALSE(book.cancelOrder(42)); // Already cancelled
}

TEST(test_modify_order)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* o = pool.allocate(); *o = {7, 10000, 50, Side::Sell};
    book.addOrder(o);

    ASSERT_TRUE(book.modifyOrder(7, 200));
    ASSERT_EQ(o->quantity, 200);

    ASSERT_TRUE(book.modifyOrder(7, 0));  // Cancel via modify
    ASSERT_EQ(book.totalOrders(), 0);
}

TEST(test_trade_reports_content)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* buy  = pool.allocate(); *buy  = {10, 10050, 25, Side::Buy};
    Order* sell = pool.allocate(); *sell = {20, 10000, 25, Side::Sell};

    book.addOrder(buy);
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].price_ticks, 10000);  // Passive (ask) price
    ASSERT_EQ(trades[0].quantity, 25);
    ASSERT_EQ(trades[0].buy_order_id, 10);
    ASSERT_EQ(trades[0].sell_order_id, 20);
    ASSERT_TRUE(trades[0].trade_id > 0);
}

// ═══════════════════════════════════════════════════════════════
//  submitOrder Tests (realistic match-on-submit)
// ═══════════════════════════════════════════════════════════════

TEST(test_submit_limit_crosses)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Rest a sell at 10000
    Order* sell = pool.allocate(); *sell = {1, 10000, 50, Side::Sell};
    book.addOrder(sell);

    // Submit a buy at 10000 — should match immediately
    Order* buy = pool.allocate(); *buy = {2, 10000, 30, Side::Buy};
    std::vector<Trade> trades;
    size_t n = book.submitOrder(buy, trades);

    ASSERT_EQ(n, 1);
    ASSERT_EQ(trades[0].quantity, 30);
    ASSERT_EQ(buy->quantity, 0);
    ASSERT_EQ(sell->quantity, 20);    // 50 - 30
    ASSERT_EQ(book.totalOrders(), 1); // Sell still resting
}

TEST(test_submit_limit_rests)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Submit a buy with no matching sell — should rest
    Order* buy = pool.allocate(); *buy = {1, 10000, 50, Side::Buy};
    std::vector<Trade> trades;
    size_t n = book.submitOrder(buy, trades);

    ASSERT_EQ(n, 0);
    ASSERT_EQ(book.totalOrders(), 1);
    ASSERT_EQ(book.bidLevelCount(), 1);
}

// ═══════════════════════════════════════════════════════════════
//  Market / IOC / FOK Tests
// ═══════════════════════════════════════════════════════════════

TEST(test_market_order_fills)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Set up liquidity: sells at 10000 and 10001
    Order* s1 = pool.allocate(); *s1 = {1, 10000, 20, Side::Sell};
    Order* s2 = pool.allocate(); *s2 = {2, 10001, 30, Side::Sell};
    book.addOrder(s1);
    book.addOrder(s2);

    // Market buy for 40 — should sweep both levels
    Order* mkt = pool.allocate(); *mkt = {3, 0, 40, Side::Buy, OrderType::Market};
    std::vector<Trade> trades;
    size_t n = book.submitOrder(mkt, trades);

    ASSERT_EQ(n, 2);
    ASSERT_EQ(trades[0].quantity, 20);        // Filled at 10000
    ASSERT_EQ(trades[0].price_ticks, 10000);
    ASSERT_EQ(trades[1].quantity, 20);        // Partial at 10001
    ASSERT_EQ(trades[1].price_ticks, 10001);
    ASSERT_EQ(mkt->quantity, 0);              // Fully filled
    ASSERT_EQ(book.totalOrders(), 1);         // s2 partially filled (10 left)
}

TEST(test_market_order_partial)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Only 20 available
    Order* s = pool.allocate(); *s = {1, 10000, 20, Side::Sell};
    book.addOrder(s);

    // Market buy for 50 — gets 20, the rest is cancelled
    Order* mkt = pool.allocate(); *mkt = {2, 0, 50, Side::Buy, OrderType::Market};
    std::vector<Trade> trades;
    book.submitOrder(mkt, trades);

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 20);
    ASSERT_EQ(mkt->quantity, 30);     // 30 unfilled (cancelled, not in book)
    ASSERT_EQ(book.totalOrders(), 0); // Market order not resting
}

TEST(test_ioc_order)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Sell resting at 10000
    Order* s = pool.allocate(); *s = {1, 10000, 20, Side::Sell};
    book.addOrder(s);

    // IOC buy at 10000 for 50 — fills 20, rest cancelled
    Order* ioc = pool.allocate(); *ioc = {2, 10000, 50, Side::Buy, OrderType::IOC};
    std::vector<Trade> trades;
    book.submitOrder(ioc, trades);

    ASSERT_EQ(trades.size(), 1);
    ASSERT_EQ(trades[0].quantity, 20);
    ASSERT_EQ(ioc->quantity, 30);     // Remainder cancelled
    ASSERT_EQ(book.totalOrders(), 0); // IOC not resting
}

TEST(test_fok_order_fills)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Enough liquidity
    Order* s = pool.allocate(); *s = {1, 10000, 50, Side::Sell};
    book.addOrder(s);

    // FOK buy for 50 — fully fillable
    Order* fok = pool.allocate(); *fok = {2, 10000, 50, Side::Buy, OrderType::FOK};
    std::vector<Trade> trades;
    size_t n = book.submitOrder(fok, trades);

    ASSERT_EQ(n, 1);
    ASSERT_EQ(trades[0].quantity, 50);
    ASSERT_EQ(book.totalOrders(), 0);
}

TEST(test_fok_order_rejects)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Not enough liquidity (only 20)
    Order* s = pool.allocate(); *s = {1, 10000, 20, Side::Sell};
    book.addOrder(s);

    // FOK buy for 50 — not fully fillable → reject
    Order* fok = pool.allocate(); *fok = {2, 10000, 50, Side::Buy, OrderType::FOK};
    std::vector<Trade> trades;
    size_t n = book.submitOrder(fok, trades);

    ASSERT_EQ(n, 0);                  // No trades (rejected)
    ASSERT_EQ(fok->quantity, 50);     // Untouched
    ASSERT_EQ(book.totalOrders(), 1); // Book unchanged
}

// ═══════════════════════════════════════════════════════════════
//  O(1) Cancel Verification
// ═══════════════════════════════════════════════════════════════

TEST(test_cancel_middle_of_queue)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    // Three orders at same price
    Order* o1 = pool.allocate(); *o1 = {1, 10000, 10, Side::Buy};
    Order* o2 = pool.allocate(); *o2 = {2, 10000, 10, Side::Buy};
    Order* o3 = pool.allocate(); *o3 = {3, 10000, 10, Side::Buy};

    book.addOrder(o1);
    book.addOrder(o2);
    book.addOrder(o3);
    ASSERT_EQ(book.totalOrders(), 3);

    // Cancel the middle one — O(1) via intrusive list
    ASSERT_TRUE(book.cancelOrder(2));
    ASSERT_EQ(book.totalOrders(), 2);

    // Match: should fill o1 first, then o3 (o2 is gone)
    Order* sell = pool.allocate(); *sell = {4, 10000, 20, Side::Sell};
    book.addOrder(sell);
    auto trades = book.match();

    ASSERT_EQ(trades.size(), 2);
    ASSERT_EQ(trades[0].buy_order_id, 1);  // o1 first
    ASSERT_EQ(trades[1].buy_order_id, 3);  // o3 after (o2 skipped)
}

// ═══════════════════════════════════════════════════════════════
//  Best Bid/Ask Tracking
// ═══════════════════════════════════════════════════════════════

TEST(test_best_bid_ask_tracking)
{
    MemoryPool<Order> pool(10);
    OrderBook book(9000, 11000);

    Order* b1 = pool.allocate(); *b1 = {1, 9900, 10, Side::Buy};
    Order* b2 = pool.allocate(); *b2 = {2, 10000, 10, Side::Buy};
    Order* s1 = pool.allocate(); *s1 = {3, 10100, 10, Side::Sell};
    Order* s2 = pool.allocate(); *s2 = {4, 10200, 10, Side::Sell};

    book.addOrder(b1);
    book.addOrder(b2);
    book.addOrder(s1);
    book.addOrder(s2);

    ASSERT_EQ(book.bestBid(), 10000);
    ASSERT_EQ(book.bestAsk(), 10100);

    // Cancel best bid — should update to 9900
    book.cancelOrder(2);
    ASSERT_EQ(book.bestBid(), 9900);

    // Cancel best ask — should update to 10200
    book.cancelOrder(3);
    ASSERT_EQ(book.bestAsk(), 10200);
}

// ═══════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════

int main()
{
    std::cout << "\n=== HFT Engine v3.0 Unit Tests ===\n\n";

    std::cout << "[Memory Pool]\n";
    RUN(test_pool_basic_alloc);
    RUN(test_pool_exhaustion);
    RUN(test_pool_dealloc_reuse);
    RUN(test_pool_reset);

    std::cout << "\n[Order Book — Core]\n";
    RUN(test_add_order);
    RUN(test_simple_match);
    RUN(test_partial_fill);
    RUN(test_fifo_ordering);
    RUN(test_no_match_when_spread);
    RUN(test_cancel_order);
    RUN(test_modify_order);
    RUN(test_trade_reports_content);

    std::cout << "\n[Order Book — submitOrder]\n";
    RUN(test_submit_limit_crosses);
    RUN(test_submit_limit_rests);

    std::cout << "\n[Order Types — Market / IOC / FOK]\n";
    RUN(test_market_order_fills);
    RUN(test_market_order_partial);
    RUN(test_ioc_order);
    RUN(test_fok_order_fills);
    RUN(test_fok_order_rejects);

    std::cout << "\n[Advanced]\n";
    RUN(test_cancel_middle_of_queue);
    RUN(test_best_bid_ask_tracking);

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n\n";

    return tests_failed > 0 ? 1 : 0;
}
