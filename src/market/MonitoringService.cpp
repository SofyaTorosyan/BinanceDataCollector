#include "MonitoringService.h"
#include "BinanceWebSocketClient.h"
#include <chrono>
#include <nlohmann/json.hpp>

namespace bdc::market
{

MonitoringService::MonitoringService(
    config::AppConfigPtr config,
    logging::ILoggerPtr logger,
    network::IWebSocketClientPtr webSocketClient,
    market::IAggregatorPtr aggregator,
    serialization::ISerializerPtr serializer)
    : m_config(std::move(config))
    , m_logger(std::move(logger))
    , m_webSocketClient(std::move(webSocketClient))
    , m_aggregator(std::move(aggregator))
    , m_serializer(std::move(serializer))
    , m_timer(m_timerIoc)
{
}

void MonitoringService::startMonitoring()
{
    m_logger->info("Starting monitoring service...");
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

    auto target = buildStreamTarget(m_config->tradingPairs);
    m_webSocketClient->connect(m_config->host, m_config->port, std::move(target));

    scheduleFlush();
    m_timerThread = std::thread([this]() { m_timerIoc.run(); });
}

void MonitoringService::stopMonitoring()
{
    m_logger->info("Stopping monitoring service...");
    m_webSocketClient->disconnect();
    m_timerIoc.stop();
    if (m_timerThread.joinable())
        m_timerThread.join();
}

void MonitoringService::scheduleFlush()
{
    m_timer.expires_after(std::chrono::milliseconds(m_config->serializationIntervalMs));
    m_timer.async_wait(
        [this](const boost::system::error_code& ec)
        {
            if (ec)
                return;
            const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();
            try
            {
                m_serializer->write(m_aggregator->popCompletedWindows(nowMs));
            }
            catch (const std::exception& e)
            {
                m_logger->error("Serialization error: {}", e.what());
            }
            scheduleFlush();
        });
}

void MonitoringService::onMessage(const std::string& jsonMessage)
{
    m_logger->trace("Received message: {}", jsonMessage);
    const auto tradeEventOpt = parseMessage(jsonMessage);
    if (!tradeEventOpt.has_value())
    {
        m_logger->error("Failed to parse message: {}", jsonMessage);
        return;
    }

    const auto& tradeEvent = tradeEventOpt.value();
    m_aggregator->addTrade(tradeEvent);
}

void MonitoringService::onError(const std::string& error)
{
    m_logger->error("WebSocket error: {}", error);
}

std::optional<TradeEvent> MonitoringService::parseMessage(const std::string& jsonMessage) const
{
    try
    {
        auto json = nlohmann::json::parse(jsonMessage);
        // Combined-stream messages wrap the payload: {"stream":"...","data":{...}}
        const auto& data = json.contains("data") ? json["data"] : json;

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

std::string MonitoringService::buildStreamTarget(const std::vector<std::string>& symbols) const
{
    // Combined-stream path: /stream?streams=btcusdt@trade/ethusdt@trade
    std::string path = "/stream?streams=";
    for (std::size_t i = 0; i < symbols.size(); ++i)
    {
        std::string lower = symbols[i];
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (i > 0)
            path += '/';
        path += lower + "@trade";
    }
    return path;
}

} // namespace bdc::market
