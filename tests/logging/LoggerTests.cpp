#include "ILogger.h"
#include "LogLevel.h"
#include "SpdlogLogger.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace
{

using bdc::logging::fromString;
using bdc::logging::ILogger;
using bdc::logging::LogLevel;
using bdc::logging::SpdlogLogger;

struct LogEntry
{
    LogLevel level;
    std::string message;
};

class SpyLogger : public ILogger
{
public:
    void log(LogLevel level, std::string_view message) override
    {
        entries.emplace_back(LogEntry{level, std::string(message)});
    }

    std::vector<LogEntry> entries;
};

} // namespace

TEST(ILoggerTest, InfoFormatsStringCorrectly)
{
    SpyLogger spy;
    spy.info("Hello, {}!", "world");

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::info);
    EXPECT_EQ(spy.entries[0].message, "Hello, world!");
}

TEST(ILoggerTest, DebugFormatsMultipleArguments)
{
    SpyLogger spy;
    spy.debug("x={} y={}", 42, 3.14);

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::debug);
    EXPECT_EQ(spy.entries[0].message, "x=42 y=3.14");
}

TEST(ILoggerTest, TraceUsesCorrectLevel)
{
    SpyLogger spy;
    spy.trace("tracing");

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::trace);
}

TEST(ILoggerTest, WarnUsesCorrectLevel)
{
    SpyLogger spy;
    spy.warn("a warning");

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::warn);
}

TEST(ILoggerTest, ErrorUsesCorrectLevel)
{
    SpyLogger spy;
    spy.error("an error");

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::error);
}

TEST(ILoggerTest, CriticalUsesCorrectLevel)
{
    SpyLogger spy;
    spy.critical("critical failure");

    ASSERT_EQ(spy.entries.size(), 1u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::critical);
}

TEST(ILoggerTest, FormatStringExpansionHappensBeforeDispatch)
{
    SpyLogger spy;
    spy.info("value={}", 99);

    EXPECT_EQ(spy.entries[0].message, "value=99");
    EXPECT_NE(spy.entries[0].message, "value={}");
}

TEST(ILoggerTest, MultipleCallsAreAllRecorded)
{
    SpyLogger spy;
    spy.info("first");
    spy.warn("second");
    spy.error("third");

    ASSERT_EQ(spy.entries.size(), 3u);
    EXPECT_EQ(spy.entries[0].level, LogLevel::info);
    EXPECT_EQ(spy.entries[1].level, LogLevel::warn);
    EXPECT_EQ(spy.entries[2].level, LogLevel::error);
}

// ---------------------------------------------------------------------------
// LogLevel conversion tests
// ---------------------------------------------------------------------------

TEST(LogLevelTest, FromStringAllValidValues)
{
    EXPECT_EQ(fromString("trace"), LogLevel::trace);
    EXPECT_EQ(fromString("debug"), LogLevel::debug);
    EXPECT_EQ(fromString("info"), LogLevel::info);
    EXPECT_EQ(fromString("warn"), LogLevel::warn);
    EXPECT_EQ(fromString("error"), LogLevel::error);
    EXPECT_EQ(fromString("critical"), LogLevel::critical);
    EXPECT_EQ(fromString("off"), LogLevel::off);
}

TEST(LogLevelTest, FromStringThrowsOnUnknownValue)
{
    EXPECT_THROW(fromString("verbose"), std::invalid_argument);
    EXPECT_THROW(fromString(""), std::invalid_argument);
    EXPECT_THROW(fromString("INFO"), std::invalid_argument); // case-sensitive
}

// ---------------------------------------------------------------------------
// SpdlogLogger construction and level-setting tests
// ---------------------------------------------------------------------------

TEST(SpdlogLoggerTest, ConstructsWithoutThrowing)
{
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-construct", LogLevel::info));
}

TEST(SpdlogLoggerTest, ConstructsWithEachLogLevel)
{
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-trace", LogLevel::trace));
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-debug", LogLevel::debug));
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-warn", LogLevel::warn));
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-error", LogLevel::error));
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-critical", LogLevel::critical));
    EXPECT_NO_THROW(SpdlogLogger("bdc-test-off", LogLevel::off));
}

TEST(SpdlogLoggerTest, SetLevelDoesNotThrow)
{
    SpdlogLogger logger("bdc-test-setlevel", LogLevel::info);
    EXPECT_NO_THROW(logger.setLevel(LogLevel::debug));
    EXPECT_NO_THROW(logger.setLevel(LogLevel::error));
    EXPECT_NO_THROW(logger.setLevel(LogLevel::off));
}

TEST(SpdlogLoggerTest, LogDoesNotThrowForAnyLevel)
{
    SpdlogLogger logger("bdc-test-log", LogLevel::trace);
    EXPECT_NO_THROW(logger.log(LogLevel::trace, "trace message"));
    EXPECT_NO_THROW(logger.log(LogLevel::debug, "debug message"));
    EXPECT_NO_THROW(logger.log(LogLevel::info, "info message"));
    EXPECT_NO_THROW(logger.log(LogLevel::warn, "warn message"));
    EXPECT_NO_THROW(logger.log(LogLevel::error, "error message"));
    EXPECT_NO_THROW(logger.log(LogLevel::critical, "critical message"));
}

TEST(SpdlogLoggerTest, SecondConstructionWithSameNameDoesNotThrow)
{
    // Exercises the spdlog::get() fallback path in the constructor.
    EXPECT_NO_THROW({
        SpdlogLogger a("bdc-test-double", LogLevel::info);
        SpdlogLogger b("bdc-test-double", LogLevel::debug);
    });
}

TEST(SpdlogLoggerTest, ImplementsILoggerInterface)
{
    std::unique_ptr<ILogger> logger =
        std::make_unique<SpdlogLogger>("bdc-test-iface", LogLevel::trace);
    EXPECT_NO_THROW(logger->info("message via interface"));
    EXPECT_NO_THROW(logger->warn("warn via interface: {}", 42));
}
