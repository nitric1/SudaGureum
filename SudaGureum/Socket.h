#pragma once

#include "Common.h"

namespace SudaGureum
{
    class SocketBase
    {
    public:
        virtual void asyncHandshakeAsServer(const std::function<void (const boost::system::error_code &)> &handler) = 0;
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler) = 0;
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler) = 0;
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler) = 0;
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
            const std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler) = 0;
        virtual void close() = 0;
    };

    class TcpSocket : public SocketBase
    {
    public:
        TcpSocket(boost::asio::io_service &ios);

    public:
        virtual void asyncHandshakeAsServer(const std::function<void (const boost::system::error_code &)> &handler);
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
            const std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler);
        virtual void close();

    public:
        boost::asio::ip::tcp::socket &socket();

    private:
        boost::asio::ip::tcp::socket socket_;
    };

    class TcpSslSocket : public SocketBase
    {
    public:
        TcpSslSocket(boost::asio::io_service &ios);
        TcpSslSocket(boost::asio::io_service &ios, const std::shared_ptr<boost::asio::ssl::context> &context);

    public:
        virtual void asyncHandshakeAsServer(const std::function<void (const boost::system::error_code &)> &handler);
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            const std::function<void (const boost::system::error_code &, size_t)> &handler);
        virtual void asyncConnect(const boost::asio::ip::tcp::resolver::iterator &endPointIt,
            const std::function<void(const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> &handler);
        virtual void close();

    public:
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type &socket();

    private:
        std::shared_ptr<boost::asio::ssl::context> ctx_;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream_;
    };
}
