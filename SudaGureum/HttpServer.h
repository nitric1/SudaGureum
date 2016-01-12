#pragma once

#include "HttpParser.h"
#include "MtIoService.h"
#include "Socket.h"

namespace SudaGureum
{
    class HttpServer;

    // TODO: context, HTTP pipelining
    class HttpConnection : public std::enable_shared_from_this<HttpConnection>
    {
    private:
        static const std::string WebSocketKeyConcatMagic;

    private:
        HttpConnection(const HttpConnection &) = delete;
        HttpConnection &operator =(const HttpConnection &) = delete;

    public:
        HttpConnection(HttpServer &server, bool ssl);

    public:
        ~HttpConnection();

    private:
        void startSsl();
        void read();
        void sendRaw(std::vector<uint8_t> data);
        void close();
        void setKeepAliveTimeout();
        void cancelKeepAliveTimeout();

    private:
        void sendHttpResponse(HttpResponse &&response);
        void sendBadRequestResponse();

    private:
        void handleHandshake(const boost::system::error_code &ec);
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleKeepAliveTimeout(const boost::system::error_code &ec);
        bool procHttpRequest(const HttpRequest &request);

    private:
        HttpServer &server_;
        boost::asio::io_service &ios_;
        std::shared_ptr<BufferedWriterSocketBase> socket_;

        HttpParser parser_;
        std::atomic<bool> continueRead_;
        std::array<uint8_t, 65536> bufferToRead_;

        bool upgradeWebSocket_;

        std::atomic<size_t> keepAliveCount_;
        boost::asio::deadline_timer keepAliveTimer_;

        friend class HttpServer;
    };

    class HttpServer : public MtIoService
    {
    private:
        static std::string handleGetPassword(size_t maxLength, boost::asio::ssl::context::password_purpose purpose);

    private:
        HttpServer(const HttpServer &) = delete;
        HttpServer &operator =(const HttpServer &) = delete;

    public:
        HttpServer(uint16_t port, bool ssl);

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
        std::shared_ptr<HttpConnection> nextConn_;

        friend class HttpConnection;
        friend class WebSocketConnection;
    };
}
