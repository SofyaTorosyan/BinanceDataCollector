#pragma once
#include "ILogger.h"
#include "IWebSocketClient.h"

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace bdc::network {

class BinanceWebSocketClient : public IWebSocketClient {
public:
    explicit BinanceWebSocketClient(logging::ILoggerPtr logger);
    ~BinanceWebSocketClient() override;

    void connect(std::string host, std::string port, std::string target) override;
    void disconnect() override;
    void setMessageHandler(MessageHandler handler) override;
    void setErrorHandler(ErrorHandler handler)     override;

private:
    using WsStream = boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>;
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    void onResolve(boost::beast::error_code ec,
                   boost::asio::ip::tcp::resolver::results_type results);
    void onConnect(boost::beast::error_code ec,
                   boost::asio::ip::tcp::resolver::results_type::endpoint_type ep);
    void onSslHandshake(boost::beast::error_code ec);
    void onWsHandshake(boost::beast::error_code ec);
    void doRead();
    void onRead(boost::beast::error_code ec, std::size_t bytes);
    void reportError(const std::string& msg);

    std::string m_host;
    std::string m_port;
    std::string m_target;

    boost::asio::io_context                                        m_ioc;
    boost::asio::ssl::context                                      m_sslCtx;
    Strand                                                         m_strand;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> m_workGuard;

    boost::asio::ip::tcp::resolver m_resolver;
    std::unique_ptr<WsStream>      m_ws;
    boost::beast::flat_buffer      m_buffer;

    logging::ILoggerPtr m_logger;
    MessageHandler      m_messageHandler;
    ErrorHandler        m_errorHandler;
    std::atomic<bool>   m_connected{false};
    std::thread         m_ioThread;
};

} // namespace bdc::network
