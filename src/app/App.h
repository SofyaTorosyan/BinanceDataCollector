#pragma once

#include "ILogger.h"
#include "IMonitoringService.h"

namespace bdc::app
{

class App
{
public:
    App();
    ~App();

    void run();

private:
    void keepRunningUntilSignal();

    logging::ILoggerPtr m_logger;
    market::IMonitoringServicePtr m_monitoringService;
};

} // namespace bdc::app
