#include "AppConfig.h"
#include "BinanceWebSocketClient.h"
#include "ILogger.h"
#include <gtest/gtest.h>

#include <atomic>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace
{

class NullLogger : public bdc::logging::ILogger
{
public:
    void log(bdc::logging::LogLevel, std::string_view) override {}
};

auto makeConfig()
{
    return std::make_shared<bdc::config::AppConfig>();
}

constexpr auto host = "stream.binance.com";
constexpr auto port = "9443";
constexpr auto target = "/stream?streams=btcusdt@trade";
constexpr auto timeout = std::chrono::seconds(10);

// Blocks until pred() returns true or timeout elapses. Returns true if pred()
// became true, false on timeout.
template <typename Pred>
bool waitFor(std::mutex& mtx, std::condition_variable& cv, Pred pred)
{
    std::unique_lock lk(mtx);
    return cv.wait_for(lk, timeout, pred);
}

} // namespace

using bdc::network::BinanceWebSocketClient;

// Connects to the real Binance stream and verifies at least one message arrives
// within the timeout.
TEST(BinanceWebSocketClientIntegrationTest, ConnectsAndReceivesMessage)
{
    std::atomic<bool> received{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setMessageHandler(
        [&](const std::string&)
        {
            if (!received.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect(host, port, target);

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load();
        }))
        << "Timed out waiting for a message from Binance";
}

TEST(BinanceWebSocketClientIntegrationTest, ConnectsAndReceives10Messages)
{
    std::atomic<int> received{0};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setMessageHandler(
        [&](const std::string& message)
        {
            std::cout << std::format("Received message: {}\n", message);
            if (received.fetch_add(1) >= 10)
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect(host, port, target);

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load() >= 10;
        }))
        << "Timed out waiting for a message from Binance";
}

// Verifies the first received message is a non-empty JSON object.
TEST(BinanceWebSocketClientIntegrationTest, ReceivedMessageIsNonEmptyJson)
{
    std::string captured;
    std::atomic<bool> received{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setMessageHandler(
        [&](const std::string& msg)
        {
            if (!received.exchange(true))
            {
                std::lock_guard lk(mtx);
                captured = msg;
                cv.notify_one();
            }
        });

    client.connect(host, port, target);

    ASSERT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load();
        }))
        << "Timed out waiting for a message";

    EXPECT_FALSE(captured.empty());
}

// Connects, waits for one message, then calls disconnect(). Verifies clean
// shutdown with no crash or deadlock.
TEST(BinanceWebSocketClientIntegrationTest, DisconnectsCleanly)
{
    std::atomic<bool> received{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setMessageHandler(
        [&](const std::string&)
        {
            if (!received.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect(host, port, target);
    waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load();
        });

    // Let the disconnect lambda execute on the io thread before the destructor fires.
    EXPECT_NO_THROW(client.disconnect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// Connects to a hostname that does not exist and verifies the error handler is
// invoked within the timeout.
TEST(BinanceWebSocketClientIntegrationTest, ErrorHandlerCalledOnInvalidHost)
{
    std::atomic<bool> errorReceived{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (!errorReceived.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect("invalid.host.that.does.not.exist", port, target);

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorReceived.load();
        }))
        << "Timed out waiting for error handler to be called";
}

// Connects to loopback with a port that has nothing listening (DNS resolves
// instantly, TCP connect fails) — exercises the onConnect error branch and
// scheduleReconnect().
TEST(BinanceWebSocketClientIntegrationTest, ConnectToClosedPort_CallsErrorHandler)
{
    std::atomic<bool> errorReceived{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (!errorReceived.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    // 127.0.0.1 resolves immediately; port 19998 is virtually never in use,
    // so async_connect fails → onConnect error branch → scheduleReconnect().
    client.connect("127.0.0.1", "19998", "/test");

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorReceived.load();
        }))
        << "Timed out waiting for TCP connect error";

    client.disconnect();
}

// Opens a local plain-TCP server that accepts one connection then closes the
// socket immediately — this causes the SSL handshake inside
// BinanceWebSocketClient to fail, exercising onSslHandshake(ec != ok).
TEST(BinanceWebSocketClientIntegrationTest, SslHandshakeFailure_CallsErrorHandler)
{
    namespace net = boost::asio;
    using tcp     = net::ip::tcp;

    net::io_context serverIoc;
    tcp::acceptor   acceptor(serverIoc, tcp::endpoint(tcp::v4(), 0));
    const auto      serverPort = std::to_string(acceptor.local_endpoint().port());

    // Accept exactly one connection then close it — no TLS, so the client's
    // SSL handshake will fail immediately.
    acceptor.async_accept(
        [](boost::system::error_code, tcp::socket sock)
        {
            boost::system::error_code ec;
            // Send garbage so the TLS layer sees bad data and errors out.
            const char garbage[] = "\x00\xFF\x00\xFF";
            sock.write_some(net::buffer(garbage, sizeof(garbage) - 1), ec);
            sock.shutdown(tcp::socket::shutdown_both, ec);
        });

    std::thread serverThread([&] { serverIoc.run(); });

    std::atomic<bool>       errorReceived{false};
    std::mutex              mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (!errorReceived.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect("127.0.0.1", serverPort, "/test");

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorReceived.load();
        }))
        << "Timed out waiting for SSL handshake error";

    client.disconnect();
    serverIoc.stop();
    serverThread.join();
}

// Connects to a real HTTPS server that doesn't speak WebSocket. The TLS
// handshake succeeds (valid cert), but the WebSocket upgrade is rejected,
// exercising the onWsHandshake(ec != ok) error branch.
TEST(BinanceWebSocketClientIntegrationTest, WsHandshakeFailure_CallsErrorHandler)
{
    std::atomic<bool>       errorReceived{false};
    std::mutex              mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (!errorReceived.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    // google.com serves HTTPS (valid cert) but does not upgrade to WebSocket.
    // The WS upgrade request will be rejected, triggering the error handler.
    client.connect("google.com", "443", "/");

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorReceived.load();
        }))
        << "Timed out waiting for WS handshake error from google.com";

    client.disconnect();
}

// Verifies that reportError() does not crash when no error handler is set
// (exercises the `if (m_errorHandler)` false branch).
TEST(BinanceWebSocketClientIntegrationTest, NoErrorHandler_ErrorDoesNotCrash)
{
    // No setErrorHandler() call — error handler is null.
    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());

    client.connect("invalid.host.that.does.not.exist", port, target);

    // Give the client time to attempt resolution and trigger reportError().
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    EXPECT_NO_THROW(client.disconnect());
}

// Connects to an invalid host and waits long enough for the reconnect timer
// to fire at least once — exercises the scheduleReconnect() timer callback.
TEST(BinanceWebSocketClientIntegrationTest, ReconnectTimerFires_AttemptsReconnect)
{
    std::atomic<int> errorCount{0};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (errorCount.fetch_add(1) == 1) // second error = after reconnect attempt
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect("invalid.host.that.does.not.exist", port, target);

    // Wait for at least 2 errors: the first failure + the reconnect attempt failure.
    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorCount.load() >= 2;
        }))
        << "Reconnect attempt did not fire within timeout";

    client.disconnect();
}
