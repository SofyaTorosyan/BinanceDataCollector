#pragma once

#include <cstdint>
#include <limits>
#include <string>

namespace bdc::serialization
{

struct WindowStats
{
    std::string symbol;
    int64_t windowStartMs{0};
    int trades{0};
    double volume{0.0};
    double minPrice{std::numeric_limits<double>::max()};
    double maxPrice{std::numeric_limits<double>::lowest()};
    int buyCount{0};
    int sellCount{0};
};

} // namespace bdc::serialization
