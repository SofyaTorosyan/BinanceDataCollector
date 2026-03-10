#include "App.h"

#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "BinanceWebSocketClient.h"
#include "FileSerializer.h"
#include "JsonConfigReader.h"
#include "LogLevel.h"
#include "MonitoringService.h"
#include "SpdlogLogger.h"
#include "TradeAggregator.h"

namespace bdc::app
{

using namespace std::string_literals;

namespace
{
const auto ConfigFileName = "config.json"s;
}

App::App()
{
}

App::~App() = default;

void App::run()
{
    try
    {
        m_configReader = std::make_shared<config::JsonConfigReader>(ConfigFileName);
        m_appConfig = std::make_shared<config::AppConfig>(m_configReader->getConfig());

        m_logger = std::make_shared<logging::SpdlogLogger>(
            "app.log"s, logging::fromString(m_appConfig->logLevel));

        m_aggregator = std::make_shared<market::TradeAggregator>(m_appConfig);
        m_webSocketClient = std::make_shared<network::BinanceWebSocketClient>(m_logger);
        m_serializer = std::make_shared<serialization::FileSerializer>(m_appConfig);
        m_monitoringService = std::make_shared<market::MonitoringService>(
            m_appConfig, m_logger, m_webSocketClient, m_aggregator, m_serializer);

        m_monitoringService->startMonitoring();

        m_logger->info("Application started successfully.");

        keepRunningUntilSignal();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

void App::keepRunningUntilSignal()
{
    boost::asio::io_context ioc;
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](const boost::system::error_code&, int)
        {
            ioc.stop();
            m_logger->info("Application is shutting down...");
            m_monitoringService->stopMonitoring();
        });
    ioc.run();
}

} // namespace bdc::app
