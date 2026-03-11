#pragma once

#include "ILogger.h"
#include "IMonitoringService.h"
#include <string>

namespace bdc::app
{

class App
{
public:
    App(int argc, char** argv);
    ~App();

    void run();

private:
    void keepRunningUntilSignal();

    std::string m_configFile;
    logging::ILoggerPtr m_logger;
    market::IMonitoringServicePtr m_monitoringService;
};

} // namespace bdc::app
