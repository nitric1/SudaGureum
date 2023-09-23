#pragma once

#include "MtIoService.h"
#include "Socket.h"
#include "WebSocketParser.h"

namespace SudaGureum
{
    class HttpServer;

    class WebSocketContext : private boost::noncopyable
    {
    public:
        WebSocketContext();

    public:
        bool authorized() const;
        const std::string &userId() const;

    private:
        std::string userId_;
    };

    class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection>
    {
    private:
        static const std::string KeyConcatMagic;

    private:
        WebSocketConnection(const WebSocketConnection &) = delete;
        WebSocketConnection &operator =(const WebSocketConnection &) = delete;

    private:
        // do I really need this?
        WebSocketConnection(HttpServer &server, bool ssl);
        // for HttpConnection
        // TODO: context
        WebSocketConnection(HttpServer &server,
            std::shared_ptr<BufferedWriterSocketBase> socket, const std::vector<uint8_t> &readBuffer);

    public:
        ~WebSocketConnection();

    public:
        void sendWebSocketResponse(const WebSocketResponse &response);
        void sendSudaGureumResponse(SudaGureumResponse &&response);

    private:
        void startSsl();
        void read();
        void sendRaw(std::vector<uint8_t> data);
        void close();

    private:
        void handleHandshake(const std::error_code &ec);
        void handleRead(const std::error_code &ec, size_t bytesTransferred);
        void handleWrite(const std::error_code &ec, size_t bytesTransferred);
        void handleCloseTimeout(const std::error_code &ec);
        void procWebSocketRequest(const WebSocketRequest &request);
        void procSudaGureumRequest(const SudaGureumRequest &request);

    private:
        HttpServer &server_;
        asio::io_service &ios_;
        std::shared_ptr<BufferedWriterSocketBase> socket_;

        WebSocketParser parser_;
        std::array<uint8_t, 65536> bufferToRead_;

        bool closeReady_;
        asio::basic_waitable_timer<std::chrono::steady_clock> closeTimer_;
        bool closeReceived_;

        friend class HttpConnection;
    };
}
