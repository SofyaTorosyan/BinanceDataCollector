#include "TradeAggregator.h"
#include <gtest/gtest.h>
#include <limits>
#include <thread>

namespace
{

using bdc::market::TradeAggregator;
using bdc::market::TradeEvent;
using bdc::serialization::WindowStats;

constexpr int kWindowMs = 1000;

TradeEvent makeTrade(std::string symbol, double price, double quantity, int64_t timeMs,
                     bool isBuyerMaker = false)
{
    TradeEvent e;
    e.symbol      = std::move(symbol);
    e.price       = price;
    e.quantity    = quantity;
    e.tradeTimeMs = timeMs;
    e.isBuyerMaker = isBuyerMaker;
    return e;
}

class TradeAggregatorTest : public ::testing::Test
{
protected:
    TradeAggregator agg{kWindowMs};
};

// --- popCompletedWindows: basic cases ---

TEST_F(TradeAggregatorTest, NoTrades_PopReturnsEmpty)
{
    auto result = agg.popCompletedWindows(99999);
    EXPECT_TRUE(result.empty());
}

TEST_F(TradeAggregatorTest, ActiveWindow_NotReturnedBeforeComplete)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    // Window [0, 1000). Completed when nowMs >= 1000.
    auto result = agg.popCompletedWindows(999);
    EXPECT_TRUE(result.empty());
}

TEST_F(TradeAggregatorTest, WindowCompletesExactlyAtBoundary)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    // windowStartMs=0, windowMs=1000 → complete when 0+1000 <= nowMs
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "BTCUSDT");
}

TEST_F(TradeAggregatorTest, CompletedWindowRemovedOnPop)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    agg.popCompletedWindows(1000);
    auto result = agg.popCompletedWindows(2000);
    EXPECT_TRUE(result.empty());
}

// --- addTrade: window assignment ---

TEST_F(TradeAggregatorTest, TradesInSameWindowMerge)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 200.0, 2.0, 999));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 2);
}

TEST_F(TradeAggregatorTest, TradesInDifferentWindowsAreDistinct)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 0);
}

TEST_F(TradeAggregatorTest, SecondWindowReturnedAfterItsCompletion)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    agg.popCompletedWindows(1000);
    auto result = agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 1000);
}

// --- addTrade: field accumulation ---

TEST_F(TradeAggregatorTest, SymbolAndWindowStartSetCorrectly)
{
    agg.addTrade(makeTrade("ETHUSDT", 1800.0, 0.5, 2500));
    auto result = agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "ETHUSDT");
    EXPECT_EQ(result[0].windowStartMs, 2000);
}

TEST_F(TradeAggregatorTest, TradesCountAccumulates)
{
    for (int i = 0; i < 5; ++i)
        agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, i * 100));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 5);
}

TEST_F(TradeAggregatorTest, VolumeAccumulates)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.5, 0));
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 2.5, 100));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].volume, 4.0);
}

TEST_F(TradeAggregatorTest, MinPriceTracked)
{
    agg.addTrade(makeTrade("BTCUSDT", 300.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100));
    agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 200));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 100.0);
}

TEST_F(TradeAggregatorTest, MaxPriceTracked)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 300.0, 1.0, 100));
    agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 200));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].maxPrice, 300.0);
}

TEST_F(TradeAggregatorTest, SingleTrade_MinEqualsMaxEqualsPrice)
{
    agg.addTrade(makeTrade("BTCUSDT", 42.0, 1.0, 0));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 42.0);
    EXPECT_DOUBLE_EQ(result[0].maxPrice, 42.0);
}

// --- addTrade: buy/sell classification ---

TEST_F(TradeAggregatorTest, IsBuyerMakerFalse_CountsAsBuy)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, /*isBuyerMaker=*/false));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 1);
    EXPECT_EQ(result[0].sellCount, 0);
}

TEST_F(TradeAggregatorTest, IsBuyerMakerTrue_CountsAsSell)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, /*isBuyerMaker=*/true));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 0);
    EXPECT_EQ(result[0].sellCount, 1);
}

TEST_F(TradeAggregatorTest, MixedBuySell_CountedCorrectly)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, false)); // buy
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100, true));  // sell
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 200, false)); // buy
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 2);
    EXPECT_EQ(result[0].sellCount, 1);
}

// --- multiple symbols ---

TEST_F(TradeAggregatorTest, DifferentSymbols_TrackSeparately)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("ETHUSDT", 200.0, 2.0, 0));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 2u);

    bool hasBtc = false, hasEth = false;
    for (const auto& w : result)
    {
        if (w.symbol == "BTCUSDT") { hasBtc = true; EXPECT_DOUBLE_EQ(w.minPrice, 100.0); }
        if (w.symbol == "ETHUSDT") { hasEth = true; EXPECT_DOUBLE_EQ(w.minPrice, 200.0); }
    }
    EXPECT_TRUE(hasBtc);
    EXPECT_TRUE(hasEth);
}

TEST_F(TradeAggregatorTest, DifferentSymbols_TradesCountedIndependently)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100));
    agg.addTrade(makeTrade("ETHUSDT", 200.0, 1.0, 0));
    auto result = agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 2u);

    for (const auto& w : result)
    {
        if (w.symbol == "BTCUSDT") EXPECT_EQ(w.trades, 2);
        if (w.symbol == "ETHUSDT") EXPECT_EQ(w.trades, 1);
    }
}

// --- window boundary arithmetic ---

TEST_F(TradeAggregatorTest, WindowStartComputedByTruncation)
{
    // tradeTimeMs=1999, windowMs=1000 → windowStart = (1999/1000)*1000 = 1000
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1999));
    auto result = agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 1000);
}

TEST_F(TradeAggregatorTest, MultipleCompletedWindowsReturnedAtOnce)
{
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1000));
    agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 2000));
    // At nowMs=3000 all three windows are complete
    auto result = agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 3u);
}

} // namespace
