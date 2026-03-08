#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

namespace bdc::logging
{

enum class LogLevel
{
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    critical = 5,
    off = 6,
};

// Converts the string values accepted by AppConfig::logLevel.
// Throws std::invalid_argument on unrecognised input.
inline LogLevel fromString(std::string_view s)
{
    if (s == "trace")
        return LogLevel::trace;
    if (s == "debug")
        return LogLevel::debug;
    if (s == "info")
        return LogLevel::info;
    if (s == "warn")
        return LogLevel::warn;
    if (s == "error")
        return LogLevel::error;
    if (s == "critical")
        return LogLevel::critical;
    if (s == "off")
        return LogLevel::off;
    throw std::invalid_argument("Unknown log level: " + std::string(s));
}

} // namespace bdc::logging
