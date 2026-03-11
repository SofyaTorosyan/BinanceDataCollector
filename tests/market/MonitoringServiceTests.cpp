#include "AppConfig.h"
#include "IAggregator.h"
#include "ILogger.h"
#include "ISerializer.h"
#include "IWebSocketClient.h"
#include "MonitoringService.h"
#include "TradeEvent.h"
#include "WindowStats.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace
{

using namespace testing;
using bdc::config::AppConfig;
using bdc::market::IAggregator;
using bdc::market::MonitoringService;
using bdc::market::TradeEvent;
using bdc::network::IWebSocketClient;
using bdc::serialization::ISerializer;
using bdc::serialization::WindowStats;

// ── Mocks ────────────────────────────────────────────────────────────────────

class MockLogger : public bdc::logging::ILogger
{
public:
    void log(bdc::logging::LogLevel, std::string_view) override {}
};

class MockWebSocketClient : public IWebSocketClient
{
public:
    MOCK_METHOD(void, connect, (std::string, std::string, std::string), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(void, setMessageHandler, (IWebSocketClient::MessageHandler), (override));
    MOCK_METHOD(void, setErrorHandler, (IWebSocketClient::ErrorHandler), (override));
};

class MockAggregator : public IAggregator
{
public:
    MOCK_METHOD(void, addTrade, (const TradeEvent&), (override));
    MOCK_METHOD(
        std::vector<WindowStats>, popCompletedWindows, (int64_t), (override));
    MOCK_METHOD(std::vector<WindowStats>, popAllWindows, (), (override));
};

class MockSerializer : public ISerializer
{
public:
    MOCK_METHOD(void, write, (const std::vector<WindowStats>&), (override));
};

// ── Fixture ───────────────────────────────────────────────────────────────────

class MonitoringServiceTest : public Test
{
protected:
    void SetUp() override
    {
        m_config = std::make_shared<AppConfig>(AppConfig{
            .tradingPairs            = {"BTCUSDT", "ETHUSDT"},
            .host                    = "stream.binance.com",
            .port                    = "9443",
            .serializationIntervalMs = 100'000, // large — prevents timer firing during tests
        });

        m_logger     = std::make_shared<MockLogger>();
        m_mockClient = std::make_shared<NiceMock<MockWebSocketClient>>();
        m_mockAgg    = std::make_shared<NiceMock<MockAggregator>>();
        m_mockSer    = std::make_shared<NiceMock<MockSerializer>>();

        // Sensible defaults so tests that don't care don't need to set them.
        ON_CALL(*m_mockAgg, popAllWindows()).WillByDefault(Return(std::vector<WindowStats>{}));
        ON_CALL(*m_mockAgg, popCompletedWindows(_))
            .WillByDefault(Return(std::vector<WindowStats>{}));
    }

    // Creates a service and returns it together with the captured WS handlers.
    // Calls startMonitoring(), so a matching stopMonitoring() must follow in
    // every test that uses this helper.
    struct StartedService
    {
        std::unique_ptr<MonitoringService> service;
        IWebSocketClient::MessageHandler  onMessage;
        IWebSocketClient::ErrorHandler    onError;
    };

    StartedService startService()
    {
        StartedService s;
        s.service = std::make_unique<MonitoringService>(
            m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

        EXPECT_CALL(*m_mockClient, setMessageHandler(_))
            .WillOnce(SaveArg<0>(&s.onMessage));
        EXPECT_CALL(*m_mockClient, setErrorHandler(_))
            .WillOnce(SaveArg<0>(&s.onError));
        EXPECT_CALL(*m_mockClient, connect(_, _, _));

        s.service->startMonitoring();
        return s;
    }

    std::shared_ptr<AppConfig>                    m_config;
    std::shared_ptr<MockLogger>                   m_logger;
    std::shared_ptr<NiceMock<MockWebSocketClient>> m_mockClient;
    std::shared_ptr<NiceMock<MockAggregator>>      m_mockAgg;
    std::shared_ptr<NiceMock<MockSerializer>>      m_mockSer;
};

// ── buildStreamTarget (tested via the target passed to connect()) ─────────────

TEST_F(MonitoringServiceTest, StartMonitoring_SingleSymbol_BuildsCorrectTarget)
{
    m_config->tradingPairs = {"BTCUSDT"};
    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(*m_mockClient, connect(_, _, "/stream?streams=btcusdt@trade"));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();
    svc->stopMonitoring();
}

TEST_F(MonitoringServiceTest, StartMonitoring_MultipleSymbols_JoinsWithSlash)
{
    m_config->tradingPairs = {"BTCUSDT", "ETHUSDT", "SOLUSDT"};
    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(
        *m_mockClient,
        connect(_, _, "/stream?streams=btcusdt@trade/ethusdt@trade/solusdt@trade"));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();
    svc->stopMonitoring();
}

TEST_F(MonitoringServiceTest, StartMonitoring_SymbolsLowercased)
{
    m_config->tradingPairs = {"BTCUSDT"};
    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    std::string capturedTarget;
    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(*m_mockClient, connect(_, _, _)).WillOnce(SaveArg<2>(&capturedTarget));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();
    svc->stopMonitoring();

    EXPECT_EQ(capturedTarget, "/stream?streams=btcusdt@trade");
    // No uppercase letters in the path after the fixed prefix.
    EXPECT_EQ(capturedTarget.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), std::string::npos);
}

// ── startMonitoring connects to the configured host/port ─────────────────────

TEST_F(MonitoringServiceTest, StartMonitoring_UsesConfigHostAndPort)
{
    m_config->host = "testhost";
    m_config->port = "1234";
    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(*m_mockClient, connect("testhost", "1234", _));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();
    svc->stopMonitoring();
}

// ── parseMessage: valid trade JSON ───────────────────────────────────────────

TEST_F(MonitoringServiceTest, OnMessage_ValidTradeJson_AddsCorrectTradeEvent)
{
    auto s = startService();

    TradeEvent captured;
    EXPECT_CALL(*m_mockAgg, addTrade(_)).WillOnce(SaveArg<0>(&captured));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    s.onMessage(R"({"s":"BTCUSDT","p":"43000.5","q":"0.01","T":1700000000000,"m":false})");

    s.service->stopMonitoring();

    EXPECT_EQ(captured.symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(captured.price, 43000.5);
    EXPECT_DOUBLE_EQ(captured.quantity, 0.01);
    EXPECT_EQ(captured.tradeTimeMs, 1700000000000LL);
    EXPECT_FALSE(captured.isBuyerMaker);
}

TEST_F(MonitoringServiceTest, OnMessage_IsBuyerMakerTrue_Forwarded)
{
    auto s = startService();

    TradeEvent captured;
    EXPECT_CALL(*m_mockAgg, addTrade(_)).WillOnce(SaveArg<0>(&captured));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    s.onMessage(R"({"s":"ETHUSDT","p":"2000.0","q":"0.5","T":1000,"m":true})");

    s.service->stopMonitoring();

    EXPECT_TRUE(captured.isBuyerMaker);
}

// ── parseMessage: combined-stream envelope ───────────────────────────────────

TEST_F(MonitoringServiceTest, OnMessage_CombinedStreamEnvelope_UnwrapsDataField)
{
    auto s = startService();

    TradeEvent captured;
    EXPECT_CALL(*m_mockAgg, addTrade(_)).WillOnce(SaveArg<0>(&captured));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    s.onMessage(
        R"({"stream":"btcusdt@trade","data":{"s":"BTCUSDT","p":"50000.0","q":"0.1","T":2000,"m":false}})");

    s.service->stopMonitoring();

    EXPECT_EQ(captured.symbol, "BTCUSDT");
    EXPECT_DOUBLE_EQ(captured.price, 50000.0);
}

// ── parseMessage: invalid / unexpected input ─────────────────────────────────

TEST_F(MonitoringServiceTest, OnMessage_InvalidJson_NoTradeAddedNoThrow)
{
    auto s = startService();

    EXPECT_CALL(*m_mockAgg, addTrade(_)).Times(0);
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    EXPECT_NO_THROW(s.onMessage("not valid json {{{"));

    s.service->stopMonitoring();
}

TEST_F(MonitoringServiceTest, OnMessage_EmptyString_NoTradeAddedNoThrow)
{
    auto s = startService();

    EXPECT_CALL(*m_mockAgg, addTrade(_)).Times(0);
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    EXPECT_NO_THROW(s.onMessage(""));

    s.service->stopMonitoring();
}

// ── stopMonitoring: without a prior startMonitoring() call ────────────────────

// stopMonitoring() must work even if startMonitoring() was never called.
// In that case m_timerThread is default-constructed (not joinable), exercising
// the false branch of `if (m_timerThread.joinable())`.
TEST_F(MonitoringServiceTest, StopMonitoring_WithoutStart_DoesNotCrash)
{
    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    EXPECT_NO_THROW(svc->stopMonitoring());
}

// ── stopMonitoring: disconnects and flushes remaining windows ─────────────────

TEST_F(MonitoringServiceTest, StopMonitoring_CallsDisconnect)
{
    auto s = startService();

    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    s.service->stopMonitoring();
}

TEST_F(MonitoringServiceTest, StopMonitoring_FlushesRemainingWindows)
{
    auto s = startService();

    WindowStats w;
    w.symbol       = "BTCUSDT";
    w.trades       = 3;
    w.windowStartMs = 1000;

    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows())
        .WillOnce(Return(std::vector<WindowStats>{w}));
    EXPECT_CALL(*m_mockSer, write(ElementsAre(Field(&WindowStats::symbol, "BTCUSDT"))));

    s.service->stopMonitoring();
}

// ── onError: error handler invocation ────────────────────────────────────────

TEST_F(MonitoringServiceTest, OnError_LogsError_NoThrow)
{
    auto s = startService();

    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    EXPECT_NO_THROW(s.onError("WebSocket connection lost"));

    s.service->stopMonitoring();
}

// ── stopMonitoring: serializer exception is caught and does not propagate ─────

TEST_F(MonitoringServiceTest, StopMonitoring_SerializerThrows_DoesNotPropagate)
{
    auto s = startService();

    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));
    EXPECT_CALL(*m_mockSer, write(_)).WillOnce(Throw(std::runtime_error("disk full")));

    EXPECT_NO_THROW(s.service->stopMonitoring());
}

// ── scheduleFlush: periodic timer fires and calls popCompletedWindows ─────────

// When the serializer throws inside the timer callback the exception must be
// caught and the service must continue running (no crash, no propagation).
TEST_F(MonitoringServiceTest, ScheduleFlush_SerializerThrows_ExceptionCaughtInCallback)
{
    m_config->serializationIntervalMs = 5;

    std::atomic<bool> timerThrew{false};
    ON_CALL(*m_mockSer, write(_)).WillByDefault([&](const std::vector<WindowStats>&) {
        timerThrew.store(true, std::memory_order_release);
        throw std::runtime_error("write failed");
    });

    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(*m_mockClient, connect(_, _, _));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!timerThrew.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // stopMonitoring also calls write() (which throws) — must not propagate.
    EXPECT_NO_THROW(svc->stopMonitoring());
    EXPECT_TRUE(timerThrew.load()) << "Timer callback never threw";
}

TEST_F(MonitoringServiceTest, ScheduleFlush_PeriodicFlush_CallsPopCompletedWindows)
{
    m_config->serializationIntervalMs = 5; // fire quickly

    std::atomic<bool> flushed{false};
    ON_CALL(*m_mockAgg, popCompletedWindows(_))
        .WillByDefault([&](int64_t) -> std::vector<WindowStats> {
            flushed.store(true, std::memory_order_release);
            return {};
        });

    auto svc = std::make_unique<MonitoringService>(
        m_config, m_logger, m_mockClient, m_mockAgg, m_mockSer);

    EXPECT_CALL(*m_mockClient, setMessageHandler(_));
    EXPECT_CALL(*m_mockClient, setErrorHandler(_));
    EXPECT_CALL(*m_mockClient, connect(_, _, _));
    EXPECT_CALL(*m_mockClient, disconnect());
    EXPECT_CALL(*m_mockAgg, popAllWindows()).WillOnce(Return(std::vector<WindowStats>{}));

    svc->startMonitoring();

    // Wait up to 2 s for the timer callback to fire at least once.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!flushed.load(std::memory_order_acquire) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    svc->stopMonitoring();

    EXPECT_TRUE(flushed.load()) << "scheduleFlush callback never fired";
}

} // namespace
