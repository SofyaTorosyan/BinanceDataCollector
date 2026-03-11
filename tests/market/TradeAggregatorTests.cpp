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
    // At nowMs=1000 only the first window [0,1000) is complete; the second [1000,2000) is not.
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    auto result = m_agg.popCompletedWindows(1000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 1);
}

TEST_F(TradeAggregatorTest, SecondWindowReturnedAfterItsCompletion)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    m_agg.popCompletedWindows(1000);
    auto result = m_agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 1);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 200.0);
}

// --- addTrade: field accumulation ---

TEST_F(TradeAggregatorTest, SymbolSetCorrectly)
{
    m_agg.addTrade(makeTrade("ETHUSDT", 1800.0, 0.5, 2500));
    auto result = m_agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "ETHUSDT");
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

TEST_F(TradeAggregatorTest, WindowBoundaryRespected_TradeInWindowIncludedWhenComplete)
{
    // tradeTimeMs=1999 belongs to window [1000,2000); complete at nowMs=2000 but not 1999.
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1999));
    EXPECT_TRUE(m_agg.popCompletedWindows(1999).empty());
    auto result = m_agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "BTCUSDT");
}

TEST_F(TradeAggregatorTest, MultipleCompletedWindowsMergedPerSymbol)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1000));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 2000));
    // Three completed windows for BTCUSDT → merged into one result entry.
    auto result = m_agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].trades, 3);
}

// --- popAllWindows ---

TEST_F(TradeAggregatorTest, PopAllWindows_EmptyAggregator_ReturnsEmpty)
{
    EXPECT_TRUE(m_agg.popAllWindows().empty());
}

TEST_F(TradeAggregatorTest, PopAllWindows_ReturnsIncompleteWindow)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 500));
    // Window [0,1000) is not yet complete at nowMs=999, but popAllWindows returns it anyway.
    auto result = m_agg.popAllWindows();
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].symbol, "BTCUSDT");
}

TEST_F(TradeAggregatorTest, PopAllWindows_ClearsAggregator)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.popAllWindows();
    EXPECT_TRUE(m_agg.popAllWindows().empty());
    EXPECT_TRUE(m_agg.popCompletedWindows(99999).empty());
}

TEST_F(TradeAggregatorTest, PopAllWindows_ReturnsOneEntryPerSymbol)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 1000)); // second window, same symbol
    m_agg.addTrade(makeTrade("ETHUSDT", 200.0, 1.0, 0));
    auto result = m_agg.popAllWindows();
    // Two BTCUSDT windows + one ETHUSDT window → merged into 2 entries.
    ASSERT_EQ(result.size(), 2u);
    for (const auto& w : result)
    {
        if (w.symbol == "BTCUSDT")
        {
            EXPECT_EQ(w.trades, 2);
        }
        if (w.symbol == "ETHUSDT")
        {
            EXPECT_EQ(w.trades, 1);
        }
    }
}

// --- mergeBySymbol: min/max tracking across multiple windows (else branch) ---

// Three completed windows for the same symbol with varying prices.
// The mergeBySymbol else-branch must correctly update min/max when merging
// the 2nd and 3rd windows, exercising both the "new min smaller" and
// "new min not smaller" branches of std::min, and analogously for std::max.
TEST_F(TradeAggregatorTest, MergeBySymbol_MinMaxTrackedAcrossWindows)
{
    // Window 0-1000: price 100  → initial min=100, max=100
    // Window 1000-2000: price 200 → merge: min stays 100 (200 >= 100), max grows to 200
    // Window 2000-3000: price 50  → merge: min shrinks to 50,  max stays 200
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 1.0, 0));
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000));
    m_agg.addTrade(makeTrade("BTCUSDT", 50.0, 1.0, 2000));
    auto result = m_agg.popCompletedWindows(3000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].minPrice, 50.0);
    EXPECT_DOUBLE_EQ(result[0].maxPrice, 200.0);
    EXPECT_EQ(result[0].trades, 3);
}

TEST_F(TradeAggregatorTest, MergeBySymbol_VolumeAndCountsAccumulate)
{
    m_agg.addTrade(makeTrade("BTCUSDT", 100.0, 2.0, 0, false));    // window 0: buy, vol=200
    m_agg.addTrade(makeTrade("BTCUSDT", 200.0, 1.0, 1000, true));  // window 1: sell, vol=200
    auto result = m_agg.popCompletedWindows(2000);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_DOUBLE_EQ(result[0].volume, 400.0);
    EXPECT_EQ(result[0].buyCount, 1);
    EXPECT_EQ(result[0].sellCount, 1);
    EXPECT_EQ(result[0].trades, 2);
}

} // namespace
