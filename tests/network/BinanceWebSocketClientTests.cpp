#include "AppConfig.h"
#include "BinanceWebSocketClient.h"
#include "ILogger.h"

#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace
{

class NullLogger : public bdc::logging::ILogger
{
public:
    void log(bdc::logging::LogLevel, std::string_view) override {}
};

std::shared_ptr<bdc::config::AppConfig> makeConfig()
{
    return std::make_shared<bdc::config::AppConfig>(
        bdc::config::AppConfig{.reconnectMaxDelayMs = 30000});
}

} // namespace

using bdc::network::BinanceWebSocketClient;

// disconnect() before connect() must not crash even though m_ws is null.
// A short sleep ensures the posted lambda runs on the io thread, covering
// the `if (!m_ws) return;` early-exit branch in the disconnect handler.
TEST(BinanceWebSocketClientTest, DisconnectBeforeConnect_DoesNotCrash)
{
    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    EXPECT_NO_THROW(client.disconnect());
    // Let the strand execute the posted lambda before the destructor fires.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// The object must be cleanly constructable and destructable without any
// connect() call in between.
TEST(BinanceWebSocketClientTest, ConstructAndDestroyWithoutConnect_DoesNotCrash)
{
    EXPECT_NO_THROW({
        BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    });
}
