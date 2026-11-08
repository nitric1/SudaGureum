#include "Common.h"

#include "Socket.h"

namespace SudaGureum
{
    TcpSocket::TcpSocket(asio::io_context &ioc)
        : socket_(ioc)
    {
    }

    void TcpSocket::asyncHandshakeAsServer(std::function<void (const std::error_code &)> handler)
    {
        throw(std::logic_error("asyncHandshakeAsServer is not implemented in TcpSocket"));
    }

    void TcpSocket::asyncReadSome(const asio::mutable_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        socket_.async_read_some(buffer, std::move(handler));
    }

    void TcpSocket::asyncWrite(const asio::const_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        asio::async_write(socket_, buffer, std::move(handler));
    }

    void TcpSocket::asyncWrite(const asio::mutable_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        asio::async_write(socket_, buffer, std::move(handler));
    }

    void TcpSocket::asyncConnect(asio::ip::tcp::resolver::results_type endpoints,
        std::function<void (const std::error_code &, asio::ip::tcp::endpoint)> handler)
    {
        asio::async_connect(socket_, std::move(endpoints), std::move(handler));
    }

    std::error_code TcpSocket::close()
    {
        std::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        return ec;
    }

    asio::ip::tcp::socket &TcpSocket::socket()
    {
        return socket_;
    }

    TcpSslSocket::TcpSslSocket(asio::io_context &ioc)
        : TcpSslSocket(ioc, std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23)) // Generic SSL/TLS
    {
    }

    TcpSslSocket::TcpSslSocket(asio::io_context &ioc, std::shared_ptr<asio::ssl::context> context)
        : ctx_(std::move(context))
        , stream_(ioc, *ctx_)
    {
    }

    void TcpSslSocket::asyncHandshakeAsServer(std::function<void (const std::error_code &)> handler)
    {
        stream_.async_handshake(asio::ssl::stream_base::server, handler);
    }

    void TcpSslSocket::asyncReadSome(const asio::mutable_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        stream_.async_read_some(buffer, std::move(handler));
    }

    void TcpSslSocket::asyncWrite(const asio::const_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        asio::async_write(stream_, buffer, std::move(handler));
    }

    void TcpSslSocket::asyncWrite(const asio::mutable_buffer &buffer,
        std::function<void (const std::error_code &, size_t)> handler)
    {
        asio::async_write(stream_, buffer, std::move(handler));
    }

    void TcpSslSocket::asyncConnect(asio::ip::tcp::resolver::results_type endpoints,
        std::function<void (const std::error_code &, asio::ip::tcp::endpoint)> handler)
    {
        asio::async_connect(stream_.lowest_layer(), std::move(endpoints),
            [this, handler](const std::error_code &ec, asio::ip::tcp::endpoint resolverIt)
            {
                if(ec)
                    handler(ec, resolverIt);
                else
                {
                    stream_.async_handshake(asio::ssl::stream_base::client,
                        std::bind(handler, std::placeholders::_1, resolverIt));
                }
            }
        );
    }

    asio::ssl::stream<asio::ip::tcp::socket>::lowest_layer_type &TcpSslSocket::socket()
    {
        return stream_.lowest_layer();
    }

    std::error_code TcpSslSocket::close()
    {
        std::error_code ec;
        stream_.shutdown(ec);
        socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket().close(ec);
        return ec;
    }
}
