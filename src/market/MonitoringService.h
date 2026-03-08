#pragma once

#include "ILogger.h"
#include "IMonitoringService.h"
#include "IWebSocketClient.h"
#include "TradeEvent.h"
#include <memory>
#include <optional>

namespace bdc::market
{
class MonitoringService : public IMonitoringService
{
public:
    MonitoringService(
        std::shared_ptr<network::IWebSocketClient> webSocketClient,
        std::shared_ptr<logging::ILogger> logger);
    void startMonitoring() override;

private:
    void onMessage(const std::string& jsonMessage);
    void onError(const std::string& error);
    std::optional<TradeEvent> parseMessage(const std::string& jsonMessage) const;

    std::shared_ptr<network::IWebSocketClient> m_webSocketClient;
    std::shared_ptr<logging::ILogger> m_logger;
};

} // namespace bdc::market
