#include "AppConfig.h"
#include "BinanceWebSocketClient.h"
#include "ILogger.h"

#include <gtest/gtest.h>

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

// disconnect() before connect() must not crash even though m_ws is null
// (exercises the `if (!m_ws) return;` branch in disconnect()).
TEST(BinanceWebSocketClientTest, DisconnectBeforeConnect_DoesNotCrash)
{
    BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    EXPECT_NO_THROW(client.disconnect());
}

// The object must be cleanly constructable and destructable without any
// connect() call in between.
TEST(BinanceWebSocketClientTest, ConstructAndDestroyWithoutConnect_DoesNotCrash)
{
    EXPECT_NO_THROW({
        BinanceWebSocketClient client(makeConfig(), std::make_shared<NullLogger>());
    });
}
