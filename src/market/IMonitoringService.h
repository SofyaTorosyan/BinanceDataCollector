#pragma once
#include <memory>
#include <string>

namespace bdc::market
{

class IMonitoringService
{
public:
    virtual ~IMonitoringService() = default;

    virtual void startMonitoring() = 0;
    virtual void stopMonitoring() = 0;
};

using IMonitoringServicePtr = std::shared_ptr<IMonitoringService>;

} // namespace bdc::market
