#pragma once

#include "Common.h"

namespace SudaGureum
{
    class SocketBase
    {
    public:
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler) = 0;
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler) = 0;
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
                                  const std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler) = 0;
        virtual void close() = 0;
    };

    class TcpSocket : public SocketBase
    {
    private:
        boost::asio::ip::tcp::socket socket_;
    public:
        TcpSocket(boost::asio::io_service &ios) : socket_(ios) {}

        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler);
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
                                  const std::function<void(const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler);
        virtual void close();
    };

    class TcpSslSocket : public SocketBase
    {
    private:
        boost::asio::ssl::context ctx_;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream_;
        std::function<void(const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator resolverIt)> connectHandler_;
    public:
        TcpSslSocket(boost::asio::io_service &ios) :
            ctx_(boost::asio::ssl::context::sslv23), /// Generic SSL/TLS.
            stream_(ios, ctx_) {}

        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer, const std::function<void(const boost::system::error_code &, size_t)> &handler);
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
                                  const std::function<void(const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler);
        virtual void close();
    };
}
