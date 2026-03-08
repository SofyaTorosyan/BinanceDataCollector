#pragma once
#include "AppConfig.h"
#include <memory>

namespace bdc::config
{

class IConfigReader
{
public:
    virtual ~IConfigReader() = default;
    virtual AppConfig getConfig() const = 0;
};

using IConfigReaderPtr = std::shared_ptr<IConfigReader>;

} // namespace bdc::config
