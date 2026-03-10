#include "JsonConfigReader.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

namespace
{

std::string writeTempFile(const std::string& content)
{
    auto path = std::filesystem::temp_directory_path() / "config.json";
    std::ofstream f(path);
    f << content;
    return path.string();
}

constexpr auto validJson = R"({
    "tradingPairs": ["BTCUSDT", "ETHUSDT"],
    "host": "stream.binance.com",
    "port": "9443",
    "windowMs": 3000,
    "serializationIntervalMs": 10000,
    "outputFile": "out.log",
    "logLevel": "debug",
    "reconnectMaxDelayMs": 30000
})";

} // namespace

using bdc::config::JsonConfigReader;

TEST(JsonConfigReaderTest, LoadsValidConfig)
{
    auto path = writeTempFile(validJson);
    JsonConfigReader reader(path);
    auto cfg = reader.getConfig();

    EXPECT_EQ(cfg.tradingPairs, (std::vector<std::string>{"BTCUSDT", "ETHUSDT"}));
    EXPECT_EQ(cfg.host, "stream.binance.com");
    EXPECT_EQ(cfg.port, "9443");
    EXPECT_EQ(cfg.windowMs, 3000);
    EXPECT_EQ(cfg.serializationIntervalMs, 10000);
    EXPECT_EQ(cfg.outputFile, "out.log");
    EXPECT_EQ(cfg.logLevel, "debug");
    EXPECT_EQ(cfg.reconnectMaxDelayMs, 30000);
}

TEST(JsonConfigReaderTest, GetConfigReturnsSameDataOnRepeatedCalls)
{
    auto path = writeTempFile(validJson);
    JsonConfigReader reader(path);

    auto cfg1 = reader.getConfig();
    auto cfg2 = reader.getConfig();

    EXPECT_EQ(cfg1.tradingPairs, cfg2.tradingPairs);
    EXPECT_EQ(cfg1.windowMs, cfg2.windowMs);
    EXPECT_EQ(cfg1.outputFile, cfg2.outputFile);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingFile)
{
    EXPECT_THROW(JsonConfigReader("/nonexistent/path/config.json"), std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnInvalidJson)
{
    auto path = writeTempFile("{ this is not valid json }");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingTradingPairs)
{
    auto path = writeTempFile(R"({
        "windowMs": 3000,
        "serializationIntervalMs": 10000,
        "outputFile": "out.log",
        "logLevel": "debug",
        "reconnectMaxDelayMs": 30000
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingWindowMs)
{
    auto path = writeTempFile(R"({
        "tradingPairs": ["BTCUSDT"],
        "host": "stream.binance.com",
        "port": "9443",
        "serializationIntervalMs": 10000,
        "outputFile": "out.log",
        "logLevel": "debug",
        "reconnectMaxDelayMs": 30000
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingSerializationIntervalMs)
{
    auto path = writeTempFile(R"({
        "tradingPairs": ["BTCUSDT"],
        "host": "stream.binance.com",
        "port": "9443",
        "windowMs": 3000,
        "outputFile": "out.log",
        "logLevel": "debug",
        "reconnectMaxDelayMs": 30000
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingOutputFile)
{
    auto path = writeTempFile(R"({
        "tradingPairs": ["BTCUSDT"],
        "host": "stream.binance.com",
        "port": "9443",
        "windowMs": 3000,
        "serializationIntervalMs": 10000,
        "logLevel": "debug",
        "reconnectMaxDelayMs": 30000
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingLogLevel)
{
    auto path = writeTempFile(R"({
        "tradingPairs": ["BTCUSDT"],
        "host": "stream.binance.com",
        "port": "9443",
        "windowMs": 3000,
        "serializationIntervalMs": 10000,
        "outputFile": "out.log",
        "reconnectMaxDelayMs": 30000
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, ThrowsOnMissingReconnectMaxDelayMs)
{
    auto path = writeTempFile(R"({
        "tradingPairs": ["BTCUSDT"],
        "host": "stream.binance.com",
        "port": "9443",
        "windowMs": 3000,
        "serializationIntervalMs": 10000,
        "outputFile": "out.log",
        "logLevel": "debug"
    })");
    EXPECT_THROW(JsonConfigReader{path}, std::runtime_error);
}

TEST(JsonConfigReaderTest, LoadsEmptyTradingPairs)
{
    auto path = writeTempFile(R"({
        "tradingPairs": [],
        "host": "stream.binance.com",
        "port": "9443",
        "windowMs": 1000,
        "serializationIntervalMs": 5000,
        "outputFile": "out.log",
        "logLevel": "info",
        "reconnectMaxDelayMs": 60000
    })");
    JsonConfigReader reader(path);
    EXPECT_TRUE(reader.getConfig().tradingPairs.empty());
}
