#pragma once
#include <string>
#include <cstdint>

namespace bdc::market
{

struct TradeEvent {
    std::string symbol;
    double      price{0.0};
    double      quantity{0.0};
    int64_t     tradeTimeMs{0};
    bool        isBuyerMaker{false};
};

} // namespace bdc::market
