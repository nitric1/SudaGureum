#pragma once

#include "MtIoService.h"
#include "WebSocketParser.h"

namespace SudaGureum
{
    class WebSocketServer;

    class WebSocketConnection : public boost::noncopyable, public std::enable_shared_from_this<WebSocketConnection>
    {
    private:
        WebSocketConnection(WebSocketServer &server);

    public:
        void sendMessage(const WebSocketMessage &message);

    private:
        void read();
        void write();
        void close();

    private:
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::vector<uint8_t>> &messagePtr);
        void procMessage(const WebSocketMessage &message);

    private:
        WebSocketServer &server_;
        boost::asio::io_service &ios_;
        boost::asio::ip::tcp::socket socket_;

        WebSocketParser parser_;
        std::array<uint8_t, 65536> bufferToRead_;

        std::mutex bufferWriteLock_;
        std::deque<std::vector<uint8_t>> bufferToWrite_;
        std::atomic<bool> inWrite_;

        friend class WebSocketServer;
    };

    class WebSocketServer : public boost::noncopyable, public MtIoService
    {
    public:
        WebSocketServer(uint16_t port);

    public:

    private:
        void acceptNext();

    private:
        void handleAccept(const boost::system::error_code &ec);

    private:
        boost::asio::ip::tcp::acceptor acceptor_;
        std::shared_ptr<WebSocketConnection> nextConn_;

        friend class WebSocketConnection;
    };
}
