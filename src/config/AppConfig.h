#pragma once
#include <memory>
#include <string>
#include <vector>

namespace bdc::config
{

struct AppConfig
{
    std::vector<std::string> tradingPairs;
    std::string host{"stream.binance.com"};
    std::string port{"9443"};
    int windowMs{1000};
    int serializationIntervalMs{5000};
    std::string outputFile{"market_data.log"};
    std::string logLevel{"info"};
    int reconnectMaxDelayMs{60000};
};

using AppConfigPtr = std::shared_ptr<AppConfig>;

} // namespace bdc::config
