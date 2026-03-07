#pragma once
#include "AppConfig.h"

namespace bdc::config
{

class IConfigReader
{
public:
    virtual ~IConfigReader() = default;
    virtual AppConfig getConfig() const = 0;
};

} // namespace bdc::config
