#pragma once

namespace bdc::market
{

class IMonitoringService
{
public:
    virtual ~IMonitoringService() = default;
    virtual void startMonitoring() = 0;
};

} // namespace bdc::market
