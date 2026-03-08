#include "MonitoringService.h"
#include "BinanceWebSocketClient.h"
#include <nlohmann/json.hpp>

namespace bdc::market
{

MonitoringService::MonitoringService(
    std::shared_ptr<network::IWebSocketClient> webSocketClient,
    std::shared_ptr<logging::ILogger> logger)
    : m_webSocketClient(std::move(webSocketClient)), m_logger(std::move(logger))
{
}

void MonitoringService::startMonitoring()
{
    m_webSocketClient->setMessageHandler(
        [this](const std::string& message)
        {
            this->onMessage(message);
        });

    m_webSocketClient->setErrorHandler(
        [this](const std::string& error)
        {
            this->onError(error);
        });

    m_webSocketClient->connect();
}

void MonitoringService::onMessage(const std::string& jsonMessage)
{
    m_logger->trace("Received message: {}", jsonMessage);
}

void MonitoringService::onError(const std::string& error)
{
    m_logger->error("WebSocket error: {}", error);
}

std::optional<TradeEvent> MonitoringService::parseMessage(const std::string& jsonMessage) const
{
    try
    {
        auto data = nlohmann::json::parse(jsonMessage);

        TradeEvent ev = {
            .symbol = data.value("s", ""),
            .price = std::stod(data.value("p", "0")),
            .quantity = std::stod(data.value("q", "0")),
            .tradeTimeMs = data.value("T", int64_t{0}),
            .isBuyerMaker = data.value("m", false),
        };

        return ev;
    }
    catch (const std::exception& error)
    {
        m_logger->warn("Message parse error: {}", error.what());
        return std::nullopt;
    }
}

} // namespace bdc::market
