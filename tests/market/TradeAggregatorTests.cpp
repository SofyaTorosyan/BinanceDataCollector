#include "AppConfig.h"
#include "TradeAggregator.h"
#include <gtest/gtest.h>
#include <limits>
#include <thread>

namespace
{

using bdc::config::AppConfig;
using bdc::market::TradeAggregator;
using bdc::market::TradeEvent;
using bdc::serialization::WindowStats;

const auto appConfig = std::make_shared<AppConfig>(AppConfig{.windowMs = 1000});

TradeEvent makeTrade(
    std::string symbol, double price, double quantity, int64_t timeMs, bool isBuyerMaker = false)
{
    TradeEvent e;
    e.symbol = std::move(symbol);
    e.price = price;
    e.quantity = quantity;
    e.tradeTimeMs = timeMs;
    e.isBuyerMaker = isBuyerMaker;
    return e;
}

class TradeAggregatorTest : public ::testing::Test
{

protected:
    TradeAggregator m_agg{appConfig};
};

// --- popCompletedWindows: basic cases ---

TEST_F(TradeAggregatorTest, NoTrades_PopReturnsEmpty)
{
    auto result = m_agg.popCompletedWindows(99999);
    EXPECT_TRUE(result.empty());
}

TEST_F(TradeAggregatorTest, ActiveWindow_NotReturnedBeforeComplete)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    // Window [0, 1000). Completed when nowMs >= 1000.
    auto result = m_agg.popCompletedWindows(999);
    EXPECT_TRUE(result.empty());
}

TEST_F(TradeAggregatorTest, WindowCompletesExactlyAtBoundary)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    // windowStartMs=0, windowMs=1000 → complete when 0+1000 <= nowMs
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "BTCUSDT");
}

TEST_F(TradeAggregatorTest, CompletedWindowRemovedOnPop)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    m_agg.popCompletedWindows(1000);
    auto result = m_agg.popCompletedWindows(2000);
    EXPECT_TRUE(result.empty());
}

// --- addTrade: window assignment ---

TEST_F(TradeAggregatorTest, TradesInSameWindowMerge)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 2.0, 999));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 2);
}

TEST_F(TradeAggregatorTest, TradesInDifferentWindowsAreDistinct)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 0);
}

TEST_F(TradeAggregatorTest, SecondWindowReturnedAfterItsCompletion)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    m_agg.popCompletedWindows(1000);
    auto result = m_agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 1000);
}

// --- addTrade: field accumulation ---

TEST_F(TradeAggregatorTest, SymbolAndWindowStartSetCorrectly)
{
    m_agg.addTrade(makeTrade("ETHUSDT", 1800.0, 0.5, 2500));
    auto result = m_agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "ETHUSDT");
    EXPECT_EQ(result[0].windowStartMs, 2000);
}

TEST_F(TradeAggregatorTest, TradesCountAccumulates)
{
    for (int i = 0; i < 5; ++i)
        m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, i * 100));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 5);
}

TEST_F(TradeAggregatorTest, VolumeAccumulates)
{
    // volume = sum(price * quantity): 100.0*1.5 + 100.0*2.5 = 400.0
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.5, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 2.5, 100));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].volume, 400.0);
}

TEST_F(TradeAggregatorTest, MinPriceTracked)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 300.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 200));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 100.0);
}

TEST_F(TradeAggregatorTest, MaxPriceTracked)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 300.0, 1.0, 100));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 200));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].maxPrice, 300.0);
}

TEST_F(TradeAggregatorTest, SingleTrade_MinEqualsMaxEqualsPrice)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 42.0, 1.0, 0));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 42.0);
    EXPECT_DOUBLE_EQ(result[0].maxPrice, 42.0);
}

// --- addTrade: buy/sell classification ---

TEST_F(TradeAggregatorTest, IsBuyerMakerFalse_CountsAsBuy)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, /*isBuyerMaker=*/false));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 1);
    EXPECT_EQ(result[0].sellCount, 0);
}

TEST_F(TradeAggregatorTest, IsBuyerMakerTrue_CountsAsSell)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, /*isBuyerMaker=*/true));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 0);
    EXPECT_EQ(result[0].sellCount, 1);
}

TEST_F(TradeAggregatorTest, MixedBuySell_CountedCorrectly)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0, false));   // buy
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100, true));  // sell
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 200, false)); // buy
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].buyCount, 2);
    EXPECT_EQ(result[0].sellCount, 1);
}

// --- multiple symbols ---

TEST_F(TradeAggregatorTest, DifferentSymbols_TrackSeparately)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("ETHUSDT", 200.0, 2.0, 0));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 2u);

    bool hasBtc = false, hasEth = false;
    for (const auto& w : result)
    {
        if (w.symbol == "BTCUSDT")
        {
            hasBtc = true;
            EXPECT_DOUBLE_EQ(w.minPrice, 100.0);
        }
        if (w.symbol == "ETHUSDT")
        {
            hasEth = true;
            EXPECT_DOUBLE_EQ(w.minPrice, 200.0);
        }
    }
    EXPECT_TRUE(hasBtc);
    EXPECT_TRUE(hasEth);
}

TEST_F(TradeAggregatorTest, DifferentSymbols_TradesCountedIndependently)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 100));
    m_agg.addTrade(makeTrade("ETHUSDT", 200.0, 1.0, 0));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 2u);

    for (const auto& w : result)
    {
        if (w.symbol == "BTCUSDT")
            EXPECT_EQ(w.trades, 2);
        if (w.symbol == "ETHUSDT")
            EXPECT_EQ(w.trades, 1);
    }
}

// --- window boundary arithmetic ---

TEST_F(TradeAggregatorTest, WindowStartComputedByTruncation)
{
    // tradeTimeMs=1999, windowMs=1000 → windowStart = (1999/1000)*1000 = 1000
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1999));
    auto result = m_agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].windowStartMs, 1000);
}

TEST_F(TradeAggregatorTest, MultipleCompletedWindowsReturnedAtOnce)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1000));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 2000));
    // At nowMs=3000 all three windows are complete
    auto result = m_agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 3u);
}

} // namespace
