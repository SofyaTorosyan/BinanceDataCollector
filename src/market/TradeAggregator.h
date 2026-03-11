#pragma once
#include "AppConfig.h"
#include "IAggregator.h"
#include <map>
#include <mutex>

namespace bdc::market
{

class TradeAggregator : public IAggregator
{
public:
    explicit TradeAggregator(config::AppConfigPtr appConfig);

    void addTrade(const TradeEvent& event) override;
    std::vector<bdc::serialization::WindowStats> popCompletedWindows(int64_t nowMs) override;
    std::vector<bdc::serialization::WindowStats> popAllWindows() override;

private:
    struct WindowKey
    {
        std::string symbol;
        int64_t windowStartMs;
        bool operator<(const WindowKey& o) const
        {
            if (symbol != o.symbol)
            {
                return symbol < o.symbol;
            }
            return windowStartMs < o.windowStartMs;
        }
    };

    static std::vector<bdc::serialization::WindowStats>
    mergeBySymbol(std::vector<bdc::serialization::WindowStats> windows);

    config::AppConfigPtr m_appConfig;
    std::map<WindowKey, bdc::serialization::WindowStats> m_windows;
    std::mutex m_mutex;
};

} // namespace bdc::market
