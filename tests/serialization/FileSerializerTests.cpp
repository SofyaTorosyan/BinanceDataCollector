#include "AppConfig.h"
#include "FileSerializer.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <sstream>
#include <string>

namespace
{

namespace fs = std::filesystem;
using bdc::config::AppConfig;
using bdc::serialization::FileSerializer;
using bdc::serialization::WindowStats;

bdc::config::AppConfigPtr makeConfig(const std::string& outputFile)
{
    auto cfg = std::make_shared<AppConfig>();
    cfg->outputFile = outputFile;
    return cfg;
}

class FileSerializerTest : public ::testing::Test
{
protected:
    fs::path m_tempFile;

    void SetUp() override
    {
        m_tempFile = fs::temp_directory_path() / "file_serializer_test.log";
        fs::remove(m_tempFile);
    }

    void TearDown() override { fs::remove(m_tempFile); }

    std::string readFile() const
    {
        std::ifstream f(m_tempFile);
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    WindowStats makeWindow(std::string symbol,
                           int64_t windowStartMs,
                           int trades,
                           double volume,
                           double minPrice,
                           double maxPrice,
                           int buyCount,
                           int sellCount)
    {
        WindowStats w;
        w.symbol = std::move(symbol);
        w.windowStartMs = windowStartMs;
        w.trades = trades;
        w.volume = volume;
        w.minPrice = minPrice;
        w.maxPrice = maxPrice;
        w.buyCount = buyCount;
        w.sellCount = sellCount;
        return w;
    }
};

} // namespace

TEST_F(FileSerializerTest, WriteEmptyVector_DoesNotCreateFile)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    s.write({});
    EXPECT_FALSE(fs::exists(m_tempFile));
}

TEST_F(FileSerializerTest, WriteAllZeroTrades_DoesNotCreateFile)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    auto w = makeWindow("BTCUSDT", 1000, 0, 0.0, 100.0, 200.0, 0, 0);
    s.write({w});
    EXPECT_FALSE(fs::exists(m_tempFile));
}

TEST_F(FileSerializerTest, WriteSingleWindow_ProducesCorrectFormat)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    auto w = makeWindow("BTCUSDT", 1609459200000LL, 5, 1.2345, 29000.1234, 30000.5678, 3, 2);
    s.write({w});

    const std::string output = readFile();
    // Timestamp is now hardware wall-clock time — just verify the header is present.
    EXPECT_NE(output.find("timestamp="), std::string::npos);
    EXPECT_NE(output.find("symbol=BTCUSDT"), std::string::npos);
    EXPECT_NE(output.find("trades=5"), std::string::npos);
    EXPECT_NE(output.find("volume=1.2345"), std::string::npos);
    EXPECT_NE(output.find("min=29000.1234"), std::string::npos);
    EXPECT_NE(output.find("max=30000.5678"), std::string::npos);
    EXPECT_NE(output.find("buy=3"), std::string::npos);
    EXPECT_NE(output.find("sell=2"), std::string::npos);
}

TEST_F(FileSerializerTest, WriteMultipleSymbolsSameTimestamp_AllUnderOneTimestampHeader)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    std::vector<WindowStats> windows = {
        makeWindow("BTCUSDT", 1000, 2, 0.5, 29000.0, 30000.0, 1, 1),
        makeWindow("ETHUSDT", 1000, 3, 10.0, 1800.0, 1900.0, 2, 1),
    };
    s.write(windows);

    const std::string output = readFile();
    // Only one timestamp header for both symbols
    const auto firstPos = output.find("timestamp=");
    const auto secondPos = output.find("timestamp=", firstPos + 1);
    EXPECT_EQ(secondPos, std::string::npos);

    EXPECT_NE(output.find("symbol=BTCUSDT"), std::string::npos);
    EXPECT_NE(output.find("symbol=ETHUSDT"), std::string::npos);
}

TEST_F(FileSerializerTest, WriteMultipleEntries_OneTimestampHeader)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    // Serializer receives pre-merged data (one entry per symbol) from the aggregator.
    std::vector<WindowStats> windows = {
        makeWindow("BTCUSDT", 0, 5, 3.5, 100.0, 200.0, 3, 2),
        makeWindow("ETHUSDT", 0, 2, 1.0, 1800.0, 1900.0, 1, 1),
    };
    s.write(windows);

    const std::string output = readFile();
    // Exactly one timestamp header for the whole flush.
    const auto firstPos = output.find("timestamp=");
    EXPECT_NE(firstPos, std::string::npos);
    EXPECT_EQ(output.find("timestamp=", firstPos + 1), std::string::npos);
    EXPECT_NE(output.find("symbol=BTCUSDT"), std::string::npos);
    EXPECT_NE(output.find("symbol=ETHUSDT"), std::string::npos);
}

TEST_F(FileSerializerTest, WriteAppendsToExistingFile)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    auto w1 = makeWindow("BTCUSDT", 1000, 1, 1.0, 100.0, 200.0, 1, 0);
    auto w2 = makeWindow("ETHUSDT", 2000, 1, 5.0, 1800.0, 1900.0, 0, 1);

    s.write({w1});
    s.write({w2});

    const std::string output = readFile();
    EXPECT_NE(output.find("symbol=BTCUSDT"), std::string::npos);
    EXPECT_NE(output.find("symbol=ETHUSDT"), std::string::npos);
}

TEST_F(FileSerializerTest, WriteSkipsWindowsWithZeroTrades)
{
    FileSerializer s(makeConfig(m_tempFile.string()));
    std::vector<WindowStats> windows = {
        makeWindow("BTCUSDT", 1000, 0, 0.0, 100.0, 200.0, 0, 0),
        makeWindow("ETHUSDT", 1000, 2, 5.0, 1800.0, 1900.0, 1, 1),
    };
    s.write(windows);

    const std::string output = readFile();
    EXPECT_EQ(output.find("symbol=BTCUSDT"), std::string::npos);
    EXPECT_NE(output.find("symbol=ETHUSDT"), std::string::npos);
}

TEST_F(FileSerializerTest, ThrowsWhenFileCannotBeOpened)
{
    FileSerializer s(makeConfig("/nonexistent/directory/output.log"));
    auto w = makeWindow("BTCUSDT", 1000, 1, 1.0, 100.0, 200.0, 1, 0);
    EXPECT_THROW(s.write({w}), std::runtime_error);
}
