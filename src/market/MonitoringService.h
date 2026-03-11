#pragma once

#include "AppConfig.h"
#include "IAggregator.h"
#include "ILogger.h"
#include "IMonitoringService.h"
#include "ISerializer.h"
#include "IWebSocketClient.h"
#include "TradeEvent.h"
#include <atomic>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <optional>
#include <thread>

namespace bdc::market
{
class MonitoringService : public IMonitoringService
{
public:
    MonitoringService(
        config::AppConfigPtr config,
        logging::ILoggerPtr logger,
        network::IWebSocketClientPtr webSocketClient,
        market::IAggregatorPtr aggregator,
        serialization::ISerializerPtr serializer);

    void startMonitoring() override;
    void stopMonitoring() override;

private:
    void onMessage(const std::string& jsonMessage);
    void onError(const std::string& error);
    std::optional<TradeEvent> parseMessage(const std::string& jsonMessage) const;
    std::string buildStreamTarget(const std::vector<std::string>& symbols) const;
    void scheduleFlush();

    config::AppConfigPtr m_config;
    logging::ILoggerPtr m_logger;
    network::IWebSocketClientPtr m_webSocketClient;
    market::IAggregatorPtr m_aggregator;
    serialization::ISerializerPtr m_serializer;

    boost::asio::io_context m_timerIoc;
    boost::asio::steady_timer m_timer;
    std::thread m_timerThread;
    std::atomic<int64_t> m_latestExchangeTimeMs{0};
};

} // namespace bdc::market
