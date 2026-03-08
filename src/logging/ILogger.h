#pragma once
#include "LogLevel.h"
#include <format>
#include <string_view>

namespace bdc::logging
{

class ILogger
{
public:
    virtual ~ILogger() = default;

    // The single virtual dispatch point. Receives a fully-formatted message.
    virtual void log(LogLevel level, std::string_view message) = 0;

    // Non-virtual convenience methods — all std::format expansion lives here.

    template <typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::trace, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::debug, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::info, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::error, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void critical(std::format_string<Args...> fmt, Args&&... args)
    {
        log(LogLevel::critical, std::format(fmt, std::forward<Args>(args)...));
    }
};

} // namespace bdc::logging
