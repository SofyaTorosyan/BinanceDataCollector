#include "FileSerializer.h"
#include <chrono>
#include <format>
#include <fstream>
#include <map>
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

FileSerializer::FileSerializer(std::string filePath) : m_filePath{std::move(filePath)}
{
}

void FileSerializer::write(const std::vector<WindowStats>& windows)
{
    if (windows.empty())
    {
        return;
    }

    // Group by windowStartMs, preserving insertion order for symbols within a group
    std::map<int64_t, std::vector<const WindowStats*>> grouped;
    for (const auto& w : windows)
    {
        if (w.trades > 0)
        {
            grouped[w.windowStartMs].push_back(&w);
        }
    }

    if (grouped.empty())
    {
        return;
    }

    std::ofstream file(m_filePath, std::ios::app);
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open output file: " + m_filePath);
    }
    file.exceptions(std::ios::failbit | std::ios::badbit);

    for (const auto& [ts, stats] : grouped)
    {
        file << std::format("timestamp={}\n", formatTimestampUtc(ts));
        for (const auto* s : stats)
        {
            file << std::format(
                "symbol={} trades={} volume={:.4f} min={:.4f} max={:.4f} buy={} sell={}\n",
                s->symbol,
                s->trades,
                s->volume,
                s->minPrice,
                s->maxPrice,
                s->buyCount,
                s->sellCount);
        }
    }
}

} // namespace bdc::serialization
