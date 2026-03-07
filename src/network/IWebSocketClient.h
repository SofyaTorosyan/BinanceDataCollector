#pragma once

#include <functional>
#include <string>

namespace bdc::network {

class IWebSocketClient {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler   = std::function<void(const std::string&)>;

    virtual ~IWebSocketClient() = default;
    virtual void connect()    = 0;
    virtual void disconnect() = 0;
    virtual void setMessageHandler(MessageHandler handler) = 0;
    virtual void setErrorHandler(ErrorHandler handler)     = 0;
};

} // namespace bdc::network
