#include "JsonConfigReader.h"
#include "AppConfig.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace
{
constexpr auto tradingPairsKey = "tradingPairs";
constexpr auto hostKey = "host";
constexpr auto portKey = "port";
constexpr auto windowMsKey = "windowMs";
constexpr auto serializationIntervalMsKey = "serializationIntervalMs";
constexpr auto outputFileKey = "outputFile";
constexpr auto logLevelKey = "logLevel";
constexpr auto reconnectMaxDelayMsKey = "reconnectMaxDelayMs";
} // namespace

namespace bdc::config
{

JsonConfigReader::JsonConfigReader(std::string_view filePath) : IConfigReader()
{
    // Load the config during construction
    m_config = load(filePath);
}

AppConfig JsonConfigReader::load(std::string_view filePath)
{
    std::ifstream file(filePath.data());
    if (!file.is_open())
    {
        throw std::runtime_error("Cannot open config file: " + std::string(filePath));
    }

    nlohmann::json jsonData;
    try
    {
        file >> jsonData;
    }
    catch (const nlohmann::json::parse_error& e)
    {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    static const char* required[] = {
        tradingPairsKey,
        hostKey,
        portKey,
        windowMsKey,
        serializationIntervalMsKey,
        outputFileKey,
        logLevelKey,
        reconnectMaxDelayMsKey};
    for (const char* key : required)
    {
        if (!jsonData.contains(key))
        {
            throw std::runtime_error(std::string("Missing required config field: ") + key);
        }
    }

    AppConfig cfg{
        .tradingPairs = jsonData[tradingPairsKey].get<std::vector<std::string>>(),
        .host = jsonData[hostKey].get<std::string>(),
        .port = jsonData[portKey].get<std::string>(),
        .windowMs = jsonData[windowMsKey].get<int>(),
        .serializationIntervalMs = jsonData[serializationIntervalMsKey].get<int>(),
        .outputFile = jsonData[outputFileKey].get<std::string>(),
        .logLevel = jsonData[logLevelKey].get<std::string>(),
        .reconnectMaxDelayMs = jsonData[reconnectMaxDelayMsKey].get<int>()};

    return cfg;
}

AppConfig JsonConfigReader::getConfig() const
{
    return m_config;
}

} // namespace bdc::config
