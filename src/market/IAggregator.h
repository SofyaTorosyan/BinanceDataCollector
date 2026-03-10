#pragma once
#include "TradeEvent.h"
#include "WindowStats.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace bdc::market
{

class IAggregator
{
public:
    virtual ~IAggregator() = default;
    virtual void addTrade(const TradeEvent& event) = 0;
    virtual std::vector<bdc::serialization::WindowStats> popCompletedWindows(int64_t nowMs) = 0;
    virtual std::vector<bdc::serialization::WindowStats> popAllWindows() = 0;
};

using IAggregatorPtr = std::shared_ptr<IAggregator>;

} // namespace bdc::market
