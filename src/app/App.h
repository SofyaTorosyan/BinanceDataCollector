#pragma once

#include "IAggregator.h"
#include "IConfigReader.h"
#include "ILogger.h"
#include "IMonitoringService.h"
#include "ISerializer.h"
#include "IWebSocketClient.h"

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

    config::IConfigReaderPtr m_configReader;
    config::AppConfigPtr m_appConfig;
    logging::ILoggerPtr m_logger;
    market::IAggregatorPtr m_aggregator;
    market::IMonitoringServicePtr m_monitoringService;
    network::IWebSocketClientPtr m_webSocketClient;
    serialization::ISerializerPtr m_serializer;
};

} // namespace bdc::app
