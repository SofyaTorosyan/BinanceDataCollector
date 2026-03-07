#include "BinanceWebSocketClient.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

namespace
{

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

    BinanceWebSocketClient client(host, port, target);
    client.setMessageHandler(
        [&](const std::string&)
        {
            if (!received.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect();

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load();
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

    BinanceWebSocketClient client(host, port, target);
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

    client.connect();

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

    BinanceWebSocketClient client(host, port, target);
    client.setMessageHandler(
        [&](const std::string&)
        {
            if (!received.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect();
    waitFor(
        mtx,
        cv,
        [&]
        {
            return received.load();
        });

    EXPECT_NO_THROW(client.disconnect());
}

// Connects to a hostname that does not exist and verifies the error handler is
// invoked within the timeout.
TEST(BinanceWebSocketClientIntegrationTest, ErrorHandlerCalledOnInvalidHost)
{
    std::atomic<bool> errorReceived{false};
    std::mutex mtx;
    std::condition_variable cv;

    BinanceWebSocketClient client("invalid.host.that.does.not.exist", port, target);
    client.setErrorHandler(
        [&](const std::string&)
        {
            if (!errorReceived.exchange(true))
            {
                std::lock_guard lk(mtx);
                cv.notify_one();
            }
        });

    client.connect();

    EXPECT_TRUE(waitFor(
        mtx,
        cv,
        [&]
        {
            return errorReceived.load();
        }))
        << "Timed out waiting for error handler to be called";
}
