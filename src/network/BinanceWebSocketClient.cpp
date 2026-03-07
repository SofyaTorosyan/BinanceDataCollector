#include "BinanceWebSocketClient.h"
#include <boost/beast/core/bind_handler.hpp>
#include <openssl/ssl.h>
#include <stdexcept>

namespace beast = boost::beast;
namespace net   = boost::asio;
namespace ssl   = boost::asio::ssl;
using tcp       = net::ip::tcp;

namespace bdc::network {

BinanceWebSocketClient::BinanceWebSocketClient(
    std::string host, std::string port, std::string target)
    : m_host(std::move(host))
    , m_port(std::move(port))
    , m_target(std::move(target))
    , m_ioc()
    , m_sslCtx(ssl::context::tlsv12_client)
    , m_strand(net::make_strand(m_ioc))
    , m_workGuard(net::make_work_guard(m_ioc))
    , m_resolver(m_strand)
{
    // Load system CA certificates for peer verification.
    m_sslCtx.set_default_verify_paths();
    m_sslCtx.set_verify_mode(ssl::verify_peer);

    // Start the io_context thread that drives all async operations.
    m_ioThread = std::thread(
        [this]()
        {
            m_ioc.run();
        });
}

BinanceWebSocketClient::~BinanceWebSocketClient()
{
    m_workGuard.reset();
    m_ioc.stop();
    if (m_ioThread.joinable())
    {
        m_ioThread.join();
    }
}

void BinanceWebSocketClient::setMessageHandler(MessageHandler handler)
{
    m_messageHandler = std::move(handler);
}

void BinanceWebSocketClient::setErrorHandler(ErrorHandler handler)
{
    m_errorHandler = std::move(handler);
}

void BinanceWebSocketClient::connect()
{
    net::post(
        m_strand,
        [this]()
        {
            m_connected = false;
            m_buffer.clear();

            m_ws = std::make_unique<WsStream>(m_strand, m_sslCtx);
            beast::get_lowest_layer(*m_ws).expires_after(std::chrono::seconds(30));

            m_resolver.async_resolve(
                m_host,
                m_port,
                beast::bind_front_handler(&BinanceWebSocketClient::onResolve, this));
        });
}

void BinanceWebSocketClient::disconnect()
{
    net::post(
        m_strand,
        [this]()
        {
            m_connected = false;
            if (!m_ws)
                return;
            m_ws->async_close(beast::websocket::close_code::normal, [](beast::error_code) {});
        });
}

void BinanceWebSocketClient::onResolve(beast::error_code ec, tcp::resolver::results_type results)
{
    if (ec)
    {
        reportError("Resolve: " + ec.message());
        return;
    }

    beast::get_lowest_layer(*m_ws).async_connect(
        results, beast::bind_front_handler(&BinanceWebSocketClient::onConnect, this));
}

void BinanceWebSocketClient::onConnect(
    beast::error_code ec, tcp::resolver::results_type::endpoint_type /*ep*/)
{
    if (ec)
    {
        reportError("Connect: " + ec.message());
        return;
    }

    // Set SNI hostname required for TLS with virtual hosting.
    if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_host.c_str()))
    {
        reportError("SSL SNI setup failed");
        return;
    }

    beast::get_lowest_layer(*m_ws).expires_after(std::chrono::seconds(30));
    m_ws->next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&BinanceWebSocketClient::onSslHandshake, this));
}

void BinanceWebSocketClient::onSslHandshake(beast::error_code ec)
{
    if (ec)
    {
        reportError("SSL handshake: " + ec.message());
        return;
    }

    // Disable TCP timeout for the long-lived WebSocket connection.
    beast::get_lowest_layer(*m_ws).expires_never();

    m_ws->set_option(beast::websocket::stream_base::timeout::suggested(beast::role_type::client));
    m_ws->set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type& req)
        {
            req.set(boost::beast::http::field::user_agent, "BinanceService/1.0");
        }));

    m_ws->async_handshake(
        m_host, m_target, beast::bind_front_handler(&BinanceWebSocketClient::onWsHandshake, this));
}

void BinanceWebSocketClient::onWsHandshake(beast::error_code ec)
{
    if (ec)
    {
        reportError("WS handshake: " + ec.message());
        return;
    }
    m_connected = true;
    doRead();
}

void BinanceWebSocketClient::doRead()
{
    m_ws->async_read(m_buffer, beast::bind_front_handler(&BinanceWebSocketClient::onRead, this));
}

void BinanceWebSocketClient::onRead(beast::error_code ec, std::size_t /*bytes*/)
{
    if (ec)
    {
        m_connected = false;
        if (ec == beast::websocket::error::closed)
        {
            return; // intentional close
        }
        reportError("Read: " + ec.message());
        return;
    }

    if (m_messageHandler)
    {
        m_messageHandler(beast::buffers_to_string(m_buffer.data()));
    }
    m_buffer.consume(m_buffer.size());
    doRead();
}

void BinanceWebSocketClient::reportError(const std::string& msg)
{
    if (!m_errorHandler)
    {
        return;
    }

    m_errorHandler(msg);
}

} // namespace bdc::network
