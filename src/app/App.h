#pragma once

#include "IAggregator.h"
#include "IConfigReader.h"
#include "ILogger.h"
#include "ISerializer.h"
#include "IWebSocketClient.h"

namespace bdc::app
{

class App
{
public:
    void run();

private:
    config::IConfigReaderPtr m_configReader;
    logging::ILoggerPtr m_logger;
    market::IAggregatorPtr m_aggregator;
    network::IWebSocketClientPtr m_webSocketClient;
    serialization::ISerializerPtr m_serializer;
};

} // namespace bdc::app
