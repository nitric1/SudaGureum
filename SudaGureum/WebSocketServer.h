#pragma once

#include "MtIoService.h"
#include "Socket.h"
#include "WebSocketParser.h"

namespace SudaGureum
{
    class WebSocketServer;

    class WebSocketConnection : private boost::noncopyable, public std::enable_shared_from_this<WebSocketConnection>
    {
    private:
        static const std::string KeyConcatMagic;

    private:
        WebSocketConnection(WebSocketServer &server, bool ssl);

    public:
        ~WebSocketConnection();

    public:
        void sendMessage(const WebSocketMessage &message);

    private:
        void startSsl();
        void read();
        void sendRaw(std::vector<uint8_t> data);
        void write();
        void close();

    private:
        void handleHandshake(const boost::system::error_code &ec);
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::vector<uint8_t>> &messagePtr);
        void handleCloseTimeout(const boost::system::error_code &ec);
        void procMessage(const WebSocketMessage &message);

    private:
        WebSocketServer &server_;
        boost::asio::io_service &ios_;
        std::shared_ptr<SocketBase> socket_;

        WebSocketParser parser_;
        std::array<uint8_t, 65536> bufferToRead_;

        std::mutex bufferWriteLock_;
        std::deque<std::vector<uint8_t>> bufferToWrite_;
        std::mutex writeLock_;
        std::atomic<bool> inWrite_;

        bool closeReady_;
        boost::asio::deadline_timer closeTimer_;
        bool closeReceived_;

        friend class WebSocketServer;
    };

    class WebSocketServer : private boost::noncopyable, public MtIoService
    {
    private:
        static std::string handleGetPassword(size_t maxLength, boost::asio::ssl::context::password_purpose purpose);

    public:
        WebSocketServer(uint16_t port, bool ssl);

    private:
        void acceptNext();
        void acceptNextSsl();

    private:
        void handleAccept(const boost::system::error_code &ec);
        void handleAcceptSsl(const boost::system::error_code &ec);

    private:
        bool ssl_;
        boost::asio::ip::tcp::acceptor acceptor_;
        std::shared_ptr<boost::asio::ssl::context> ctx_;
        std::shared_ptr<WebSocketConnection> nextConn_;

        friend class WebSocketConnection;
    };
}
