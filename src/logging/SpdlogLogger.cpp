#include "SpdlogLogger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace bdc::logging
{

namespace
{

spdlog::level::level_enum toSpdlogLevel(LogLevel level)
{
    switch (level)
    {
    case LogLevel::trace:    return spdlog::level::trace;
    case LogLevel::debug:    return spdlog::level::debug;
    case LogLevel::info:     return spdlog::level::info;
    case LogLevel::warn:     return spdlog::level::warn;
    case LogLevel::error:    return spdlog::level::err; // spdlog uses "err", not "error"
    case LogLevel::critical: return spdlog::level::critical;
    case LogLevel::off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

} // namespace

SpdlogLogger::SpdlogLogger(std::string name, LogLevel level)
{
    // Retrieve existing logger if already registered — avoids duplicate-registration
    // exceptions when multiple SpdlogLogger objects share a name (e.g., in tests).
    m_logger = spdlog::get(name);
    if (!m_logger)
    {
        m_logger = spdlog::stdout_color_mt(name);
    }
    m_logger->set_level(toSpdlogLevel(level));
}

void SpdlogLogger::log(LogLevel level, std::string_view message)
{
    m_logger->log(toSpdlogLevel(level), message);
}

void SpdlogLogger::setLevel(LogLevel level)
{
    m_logger->set_level(toSpdlogLevel(level));
}

} // namespace bdc::logging
