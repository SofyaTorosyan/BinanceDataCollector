#include "ArgParser.h"
#include <gtest/gtest.h>

using bdc::app::parseArgument;
using bdc::app::parseConfigPath;

namespace
{

// Helper to build a non-const argv array from a list of string literals.
struct Args
{
    explicit Args(std::initializer_list<const char*> args) : m_args(args) {}

    int argc() const { return static_cast<int>(m_args.size()); }
    char** argv() { return const_cast<char**>(m_args.data()); }

private:
    std::vector<const char*> m_args;
};

} // namespace

// --- parseConfigPath (thin wrapper) ---

TEST(ArgParserTest, ReturnsDefaultWhenNoArgs)
{
    Args a{"app"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "config.json");
}

TEST(ArgParserTest, ShortFlagSetsConfigPath)
{
    Args a{"app", "-c", "my.json"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "my.json");
}

TEST(ArgParserTest, LongFlagSetsConfigPath)
{
    Args a{"app", "--config", "custom/path.json"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "custom/path.json");
}

TEST(ArgParserTest, LongFlagEqualsFormSetsConfigPath)
{
    Args a{"app", "--config=another.json"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "another.json");
}

TEST(ArgParserTest, ShortFlagWithOtherArgsBefore)
{
    Args a{"app", "--verbose", "-c", "cfg.json"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "cfg.json");
}

TEST(ArgParserTest, ShortFlagWithNoValueReturnsDefault)
{
    Args a{"app", "-c"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "config.json");
}

TEST(ArgParserTest, UnknownFlagsAreIgnored)
{
    Args a{"app", "--foo", "bar", "--baz"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "config.json");
}

TEST(ArgParserTest, FirstMatchWins)
{
    Args a{"app", "-c", "first.json", "--config", "second.json"};
    EXPECT_EQ(parseConfigPath(a.argc(), a.argv()), "first.json");
}

// --- parseArgument (generic) ---

TEST(ParseArgumentTest, CustomFlagReturnsValue)
{
    Args a{"app", "--output", "out.csv"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "default.csv", {"--output"}), "out.csv");
}

TEST(ParseArgumentTest, CustomFlagEqualsForm)
{
    Args a{"app", "--output=out.csv"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "default.csv", {"--output"}), "out.csv");
}

TEST(ParseArgumentTest, MultipleFlagsAnyMatches)
{
    Args a{"app", "-o", "out.csv"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "default.csv", {"-o", "--output"}), "out.csv");
}

TEST(ParseArgumentTest, ReturnsCustomDefault)
{
    Args a{"app"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "my-default", {"--flag"}), "my-default");
}

TEST(ParseArgumentTest, ShortFlagEqualsFormNotSupported)
{
    // Equals form is only handled for long (--) flags
    Args a{"app", "-o=out.csv"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "default.csv", {"-o"}), "default.csv");
}

TEST(ParseArgumentTest, EmptyFlagListReturnsDefault)
{
    Args a{"app", "-c", "something.json"};
    EXPECT_EQ(parseArgument(a.argc(), a.argv(), "default.json", {}), "default.json");
}
