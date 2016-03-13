#pragma once

#include "Common.h"

namespace SudaGureum
{
    class SocketBase
    {
    public:
        virtual void asyncHandshakeAsServer(std::function<void (const boost::system::error_code &)> handler) = 0;
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler) = 0;
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler) = 0;
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler) = 0;
        virtual void asyncConnect(boost::asio::ip::tcp::resolver::iterator endPointIt,
            std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> handler) = 0;
        virtual boost::system::error_code close() = 0;
    };

    class TcpSocket : public SocketBase
    {
    public:
        TcpSocket(boost::asio::io_service &ios);

    public:
        virtual void asyncHandshakeAsServer(std::function<void (const boost::system::error_code &)> handler);
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncConnect(boost::asio::ip::tcp::resolver::iterator endPointIt,
            std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> handler);
        virtual boost::system::error_code close();

    public:
        boost::asio::ip::tcp::socket &socket();

    private:
        boost::asio::ip::tcp::socket socket_;
    };

    class TcpSslSocket : public SocketBase
    {
    public:
        TcpSslSocket(boost::asio::io_service &ios);
        TcpSslSocket(boost::asio::io_service &ios, std::shared_ptr<boost::asio::ssl::context> context);

    public:
        virtual void asyncHandshakeAsServer(std::function<void (const boost::system::error_code &)> handler);
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler);
        virtual void asyncConnect(boost::asio::ip::tcp::resolver::iterator endPointIt,
            std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> handler);
        virtual boost::system::error_code close();

    public:
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket>::lowest_layer_type &socket();

    private:
        std::shared_ptr<boost::asio::ssl::context> ctx_;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> stream_;
    };

    class BufferedWriterSocketBase : public SocketBase
    {
    public:
        using SocketBase::asyncWrite;

        virtual void asyncWrite(std::vector<uint8_t> data,
            std::function<void (const boost::system::error_code &, size_t)> handler) = 0;
    };

    template<typename Socket>
    class BufferedWriterSocket : public BufferedWriterSocketBase, public std::enable_shared_from_this<BufferedWriterSocket<Socket>>
    {
    public:
        template<typename ...Args>
        BufferedWriterSocket(boost::asio::io_service &ios, Args &&...args)
            : socket_(ios, std::forward<Args>(args)...)
            , inWrite_(false)
        {
        }

    public:
        virtual void asyncHandshakeAsServer(std::function<void (const boost::system::error_code &)> handler)
        {
            socket_.asyncHandshakeAsServer(std::move(handler));
        }
        virtual void asyncReadSome(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler)
        {
            socket_.asyncReadSome(buffer, std::move(handler));
        }
        virtual void asyncWrite(const boost::asio::const_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler)
        {
            socket_.asyncWrite(buffer, std::move(handler));
        }
        virtual void asyncWrite(const boost::asio::mutable_buffers_1 &buffer,
            std::function<void (const boost::system::error_code &, size_t)> handler)
        {
            socket_.asyncWrite(buffer, std::move(handler));
        }
        virtual void asyncConnect(boost::asio::ip::tcp::resolver::iterator endPointIt,
            std::function<void (const boost::system::error_code &, boost::asio::ip::tcp::resolver::iterator)> handler)
        {
            socket_.asyncConnect(endPointIt, std::move(handler));
        }
        virtual boost::system::error_code close()
        {
            return socket_.close();
        }

        virtual void asyncWrite(std::vector<uint8_t> data,
            std::function<void (const boost::system::error_code &, size_t)> handler)
        {
            {
                std::lock_guard<std::mutex> lock(bufferWriteLock_);
                bufferToWrite_.emplace_back(std::move(data), std::move(handler));
            }
            asyncWrite();
        }

    public:
        decltype(std::declval<Socket>().socket()) socket()
        {
            return socket_.socket();
        }

    private:
        void asyncWrite()
        {
            std::lock_guard<std::mutex> lock(writeLock_); // do I really need this?

            if(inWrite_.exchange(true))
            {
                return;
            }

            try
            {
                std::vector<uint8_t> *frame = nullptr;
                if(!bufferToWrite_.empty())
                {
                    frame = &bufferToWrite_.front().first;
                }

                if(frame && !frame->empty())
                {
                    socket_.asyncWrite(
                        boost::asio::buffer(*frame, frame->size()),
                        boost::bind(
                            std::mem_fn(&BufferedWriterSocket::handleWrite),
                            shared_from_this(),
                            boost::asio::placeholders::error,
                            boost::asio::placeholders::bytes_transferred));
                }
                else
                {
                    inWrite_ = false;
                }
            }
            catch(...)
            {
                inWrite_ = false;
                throw;
            }
        }

        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred)
        {
            auto handler = std::move(bufferToWrite_.front().second);

            {
                std::lock_guard<std::mutex> lock(bufferWriteLock_);
                assert(!bufferToWrite_.empty());
                bufferToWrite_.pop_front();
            }

            inWrite_ = false;

            handler(ec, bytesTransferred);

            if(!ec)
            {
                asyncWrite();
            }
        }

    private:
        Socket socket_;
        std::mutex bufferWriteLock_;
        std::deque<std::pair<
            std::vector<uint8_t>, std::function<void (const boost::system::error_code &, size_t)>>> bufferToWrite_;
        std::mutex writeLock_;
        std::atomic<bool> inWrite_;
    };
}
