#include "FileSerializer.h"
#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>

namespace bdc::serialization
{

namespace
{

std::string formatTimestampUtc(const int64_t ms)
{
    const auto timestamp =
        std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{ms}};
    return std::format(
        "{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(timestamp));
}

} // namespace

FileSerializer::FileSerializer(config::AppConfigPtr config) : m_config{std::move(config)}
{
}

void FileSerializer::write(const std::vector<WindowStats>& windows)
{
    const bool hasData = std::any_of(
        windows.begin(),
        windows.end(),
        [](const WindowStats& w)
        {
            return w.trades > 0;
        });
    if (!hasData)
        return;

    // Use hardware wall-clock time as the flush timestamp.
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    std::ofstream file(m_config->outputFile, std::ios::app);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open output file: " + m_config->outputFile);
    }
    file.exceptions(std::ios::failbit | std::ios::badbit);

    file << std::format("timestamp={}\n", formatTimestampUtc(nowMs));
    for (const auto& s : windows)
    {
        if (s.trades == 0)
        {
            continue;
        }
        file << std::format(
            "symbol={} trades={} volume={:.4f} min={:.4f} max={:.4f} buy={} sell={}\n",
            s.symbol,
            s.trades,
            s.volume,
            s.minPrice,
            s.maxPrice,
            s.buyCount,
            s.sellCount);
    }
}

} // namespace bdc::serialization
