#pragma once

#include "WindowStats.h"
#include <vector>

namespace bdc::serialization
{

class ISerializer
{
public:
    virtual ~ISerializer() = default;
    // Windows with trades == 0 are silently skipped.
    virtual void write(const std::vector<WindowStats>& windows) = 0;
};

} // namespace bdc::serialization
