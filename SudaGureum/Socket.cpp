#include "Common.h"

#include "Socket.h"

namespace SudaGureum
{
    TcpSocket::TcpSocket(boost::asio::io_service &ios)
        : socket_(ios)
    {
    }

    void TcpSocket::asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
        const std::function<void (const boost::system::error_code &, size_t)> &handler)
    {
        socket_.async_read_some(buffer, handler);
    }

    void TcpSocket::asyncWrite(const boost::asio::const_buffers_1 &buffer,
        const std::function<void (const boost::system::error_code &, size_t)> &handler)
    {
        boost::asio::async_write(socket_, buffer, handler);
    }

    void TcpSocket::asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
        const std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler)
    {
        boost::asio::async_connect(socket_, endPointIt, handler);
    }

    void TcpSocket::close()
    {
        socket_.close();
    }

    TcpSslSocket::TcpSslSocket(boost::asio::io_service &ios)
        : ctx_(boost::asio::ssl::context::sslv23) // Generic SSL/TLS
        , stream_(ios, ctx_)
    {
    }

    void TcpSslSocket::asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
        const std::function<void (const boost::system::error_code &, size_t)> &handler)
    {
        stream_.async_read_some(buffer, handler);
    }

    void TcpSslSocket::asyncWrite(const boost::asio::const_buffers_1 &buffer,
        const std::function<void (const boost::system::error_code &, size_t)> &handler)
    {
        boost::asio::async_write(stream_, buffer, handler);
    }

    void TcpSslSocket::asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
        const std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler)
    {
        boost::asio::async_connect(stream_.lowest_layer(), endPointIt,
            [this, handler](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator resolverIt)
            {
                if(ec)
                    handler(ec, resolverIt);
                else
                {
                    stream_.async_handshake(boost::asio::ssl::stream_base::client,
                        std::bind(handler, std::placeholders::_1, resolverIt));
                }
            }
        );
    }

    void TcpSslSocket::close()
    {
        stream_.shutdown();
    }
}