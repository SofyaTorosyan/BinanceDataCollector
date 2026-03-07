#pragma once
#include <string>
#include <vector>

namespace bdc::config
{

struct AppConfig
{
    std::vector<std::string> tradingPairs;
    int windowMs{1000};
    int serializationIntervalMs{5000};
    std::string outputFile{"market_data.log"};
    std::string logLevel{"info"};
    int reconnectMaxDelayMs{60000};
};

} // namespace bdc::config
