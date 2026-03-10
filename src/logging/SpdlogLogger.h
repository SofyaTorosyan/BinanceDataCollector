#pragma once
#include "ILogger.h"
#include <memory>
#include <string>

// Forward-declare to avoid pulling spdlog headers into every consumer.
namespace spdlog
{
class logger;
}

namespace bdc::logging
{

class SpdlogLogger : public ILogger
{
public:
    explicit SpdlogLogger(std::string name, LogLevel level = LogLevel::info);

    void log(LogLevel level, std::string_view message) override;

    // Changes the minimum log level at runtime.
    void setLevel(LogLevel level);

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace bdc::logging
