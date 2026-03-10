#include "TradeAggregator.h"
#include <algorithm>

namespace bdc::market
{

TradeAggregator::TradeAggregator(config::AppConfigPtr appConfig) : m_appConfig(std::move(appConfig))
{
}

void TradeAggregator::addTrade(const TradeEvent& event)
{
    int64_t windowStartMs = (event.tradeTimeMs / m_appConfig->windowMs) * m_appConfig->windowMs;
    WindowKey key{event.symbol, windowStartMs};

    std::lock_guard<std::mutex> lock(m_mutex);
    auto& stats = m_windows[key];

    if (stats.trades == 0)
    {
        stats.symbol = event.symbol;
        stats.windowStartMs = windowStartMs;
    }
    stats.trades++;
    stats.volume += event.price * event.quantity;
    stats.minPrice = std::min(stats.minPrice, event.price);
    stats.maxPrice = std::max(stats.maxPrice, event.price);
    // isBuyerMaker=true means the buyer is the passive side → aggressive side is the seller
    if (event.isBuyerMaker)
    {
        stats.sellCount++;
    }
    else
    {
        stats.buyCount++;
    }
}

std::vector<bdc::serialization::WindowStats> TradeAggregator::popCompletedWindows(int64_t nowMs)
{
    std::vector<bdc::serialization::WindowStats> result;
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_windows.begin(); it != m_windows.end();)
    {
        if (it->first.windowStartMs + m_appConfig->windowMs <= nowMs)
        {
            result.push_back(it->second);
            it = m_windows.erase(it);
        }
        else
        {
            ++it;
        }
    }
    return result;
}

std::vector<bdc::serialization::WindowStats> TradeAggregator::popAllWindows()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<bdc::serialization::WindowStats> result;
    result.reserve(m_windows.size());
    for (auto& [key, stats] : m_windows)
        result.push_back(std::move(stats));
    m_windows.clear();
    return result;
}

} // namespace bdc::market
