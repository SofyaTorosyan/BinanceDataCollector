#include "App.h"
#include "ArgParser.h"

#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/di.hpp>

#include "BinanceWebSocketClient.h"
#include "FileSerializer.h"
#include "JsonConfigReader.h"
#include "LogLevel.h"
#include "MonitoringService.h"
#include "SpdlogLogger.h"
#include "TradeAggregator.h"

namespace di = boost::di;

namespace bdc::app
{

using namespace std::string_literals;
using namespace std::string_view_literals;

namespace
{
constexpr auto logName = "app"s;
} // namespace

App::App(int argc, char** argv)
    : m_configFile{parseConfigPath(argc, argv)}
{
}

App::~App() = default;

void App::run()
{
    try
    {
        auto configReader = std::make_shared<config::JsonConfigReader>(m_configFile);
        auto appConfig = std::make_shared<config::AppConfig>(configReader->getConfig());
        auto logger = std::make_shared<logging::SpdlogLogger>(
            logName, logging::fromString(appConfig->logLevel));

        auto injector = di::make_injector(
            di::bind<config::AppConfig>().to(appConfig),
            di::bind<logging::ILogger>().to(logger),
            di::bind<network::IWebSocketClient>().to<network::BinanceWebSocketClient>(),
            di::bind<serialization::ISerializer>().to<serialization::FileSerializer>(),
            di::bind<market::IAggregator>().to<market::TradeAggregator>(),
            di::bind<market::IMonitoringService>().to<market::MonitoringService>());

        m_logger = logger;
        m_monitoringService = injector.create<std::shared_ptr<market::IMonitoringService>>();

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
