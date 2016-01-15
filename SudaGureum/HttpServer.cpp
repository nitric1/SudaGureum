#include "Common.h"

#include "HttpServer.h"

#include "Configure.h"
#include "Default.h"
#include "Log.h"
#include "Utility.h"
#include "WebSocketServer.h"

namespace SudaGureum
{
    const std::string HttpConnection::WebSocketKeyConcatMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    HttpConnection::HttpConnection(HttpServer &server, bool ssl)
        : server_(server)
        , ios_(server.ios_)
        , continueRead_(true)
        , upgradeWebSocket_(false)
        , keepAliveCount_(Configure::instance().getAs(
            "http_server_keep_alive_max_count", DefaultConfigureValue::HttpServerKeepAliveMaxCount))
        , keepAliveTimer_(ios_)
    {
        if(ssl)
            socket_ = std::make_shared<BufferedWriterSocket<TcpSslSocket>>(ios_, server.ctx_);
        else
            socket_ = std::make_shared<BufferedWriterSocket<TcpSocket>>(ios_);
    }

    HttpConnection::~HttpConnection()
    {
        Log::instance().trace("HttpConnection[{}]: connection closed", static_cast<void *>(this));
    }

    void HttpConnection::sendRaw(std::vector<uint8_t> data)
    {
        socket_->asyncWrite(data,
            boost::bind(
                std::mem_fn(&HttpConnection::handleWrite),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void HttpConnection::sendHttpResponse(HttpResponse &&response)
    {
        // TODO: reimplement w/ compression
        std::string headerField = fmt::format(
            "HTTP/1.1 {}\r\n" // TODO: status message
            "Server: SudaGureum Placeholder Server\r\n"
            "Content-Length: {}\r\n",
            response.status_,
            response.body_.size());
        for(const auto &header : response.headers_)
        {
            headerField += fmt::format("{}: {}\r\n", header.first, header.second);
        }
        headerField += "\r\n";

        std::vector<uint8_t> data(headerField.begin(), headerField.end());
        data.insert(data.end(), response.body_.begin(), response.body_.end());

        sendRaw(std::move(data));
    }

    void HttpConnection::sendBadRequestResponse()
    {
        // TODO: reimplement
        static const std::string Body = "Bad request";

        HttpResponse response;
        response.status_ = 403;
        response.headers_ =
        {
            {"Connection", "close"},
            {"Content-Type", "text/plain"},
        };
        response.body_.assign(Body.begin(), Body.end());

        sendHttpResponse(std::move(response));
    }

    void HttpConnection::startSsl()
    {
        socket_->asyncHandshakeAsServer(
            boost::bind(
                std::mem_fn(&HttpConnection::handleHandshake),
                shared_from_this(),
                boost::asio::placeholders::error));
    }

    void HttpConnection::read()
    {
        if(!socket_)
        {
            return;
        }

        socket_->asyncReadSome(
            boost::asio::buffer(bufferToRead_),
            boost::bind(
                std::mem_fn(&HttpConnection::handleRead),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void HttpConnection::close()
    {
        cancelKeepAliveTimeout();
        socket_->close();
    }

    void HttpConnection::setKeepAliveTimeout()
    {
        keepAliveTimer_.expires_from_now(boost::posix_time::seconds(
            Configure::instance().getAs("http_server_keep_alive_timeout_sec",
                DefaultConfigureValue::HttpServerKeepAliveTimeoutSec)));
        keepAliveTimer_.async_wait(boost::bind(
            std::mem_fn(&HttpConnection::handleKeepAliveTimeout),
            shared_from_this(),
            boost::asio::placeholders::error));
    }

    void HttpConnection::cancelKeepAliveTimeout()
    {
        boost::system::error_code ec;
        keepAliveTimer_.cancel(ec);
    }

    void HttpConnection::handleHandshake(const boost::system::error_code &ec)
    {
        if(ec)
        {
            Log::instance().alert("HttpConnection[{}]: handshake failed: {}", static_cast<void *>(this), ec.message());
            // socket automatically closed
            return;
        }

        read();
    }

    void HttpConnection::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(!continueRead_)
        {
            return;
        }

        cancelKeepAliveTimeout();

        if(ec)
        {
            Log::instance().alert("HttpConnection[{}]: read failed: {}", static_cast<void *>(this), ec.message());
            close(); // implies forcely canceling write
            return;
        }

        std::vector<uint8_t> data(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred);

        auto res = parser_.parse(std::vector<uint8_t>(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&HttpConnection::procHttpRequest, this, std::placeholders::_1));
        if(!res.first)
        {
            Log::instance().alert("HttpConnection[{}]: read: invalid data received", static_cast<void *>(this));
            sendBadRequestResponse();
            // do not continue read -> socket closed after writing
            return;
        }
        else if(res.second > 0)
        {
            if(upgradeWebSocket_)
            {
                Log::instance().trace("HttpConnection[{}]: read: upgrade to web socket", static_cast<void *>(this));
                data.erase(data.begin(), data.begin() + res.second);
                std::shared_ptr<WebSocketConnection> wsConn(new WebSocketConnection(server_, std::move(socket_), data));
                wsConn->handleRead(boost::system::error_code(), data.size());
            }
            else
            {
                Log::instance().alert("HttpConnection[{}]: read: wrong upgrade header or not web socket", static_cast<void *>(this));
                sendBadRequestResponse();
                // do not continue read -> socket closed after writing
            }
            return;
        }

        if(!continueRead_)
        {
            return;
        }

        read();
    }

    void HttpConnection::handleWrite(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            Log::instance().alert("HttpConnection[{}]: write failed: {}", static_cast<void *>(this), ec.message());
            close(); // implies forcely canceling read
            return;
        }
    }

    void HttpConnection::handleKeepAliveTimeout(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            Log::instance().info("HttpConnection[{}]: keep-alive timeout", static_cast<void *>(this));
            continueRead_ = false;
        }
    }

    bool HttpConnection::procHttpRequest(const HttpRequest &request)
    {
        size_t currentKeepAliveCount = keepAliveCount_ --;
        bool keepAlive = (request.keepAlive_ && currentKeepAliveCount > 0);

        Log::instance().trace("HttpConnection[{}]: http request, url={} keepAlive=R{}/A{} curKAcnt={}", // R(requested), A(applied)
            static_cast<void *>(this), request.path_, request.keepAlive_, keepAlive, currentKeepAliveCount);

        if(request.upgrade_)
        {
            if(!boost::iequals(request.headers_.at("Upgrade"), "websocket"))
            {
                return true;
            }

            // accept only WebSocket version 13
            if(request.headers_.at("Sec-WebSocket-Version").find("13") == std::string::npos)
            {
                return true;
            }

            // TODO: use HttpResponse
            static const std::string responseFormat =
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Sec-WebSocket-Accept: {}\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n";

            std::string accept = request.headers_.at("Sec-WebSocket-Key") + WebSocketKeyConcatMagic;
            auto hash = hashSha1(std::vector<uint8_t>(accept.begin(), accept.end()));
            std::string acceptHashed = encodeBase64(std::vector<uint8_t>(hash.begin(), hash.end()));

            std::string response = fmt::format(responseFormat, acceptHashed);
            sendRaw(std::vector<uint8_t>(response.begin(), response.end()));

            upgradeWebSocket_ = true;
            return true;
        }

        // TODO: process & response
        // TODO: decode path & GET/HEAD/POST parameters

        // TODO: placeholder; remove
        if(keepAlive)
        {
            static const std::string responseFormat =
                "HTTP/1.1 404 Not Found\r\n"
                "Connection: keep-alive\r\n"
                "Keep-Alive: timeout={}, max={}\r\n"
                "Server: SudaGureum Placeholder Server\r\n"
                "Content-Type: text-plain; charset=UTF-8\r\n"
                "Content-Length: 33\r\n"
                "\r\n"
                "HTTP Server Not Fully Implemented";

            std::string response = fmt::format(responseFormat,
                Configure::instance().getAs("http_server_keep_alive_timeout_sec",
                    DefaultConfigureValue::HttpServerKeepAliveTimeoutSec),
                currentKeepAliveCount);
            sendRaw(std::vector<uint8_t>(response.begin(), response.end()));
        }
        else
        {
            static const std::string response =
                "HTTP/1.1 404 Not Found\r\n"
                "Connection: close\r\n"
                "Server: SudaGureum Placeholder Server\r\n"
                "Content-Type: text-plain; charset=UTF-8\r\n"
                "Content-Length: 33\r\n"
                "\r\n"
                "HTTP Server Not Fully Implemented";
            static const std::vector<uint8_t> responseVector(response.begin(), response.end());
            sendRaw(responseVector);
        }

        if(!keepAlive)
        {
            continueRead_ = false;
            return false;
        }

        setKeepAliveTimeout();
        return true;
    }

    std::string HttpServer::handleGetPassword(size_t maxLength, boost::asio::ssl::context::password_purpose purpose)
    {
        return Configure::instance().get("ssl_certificate_password");
    }

    HttpServer::HttpServer(uint16_t port, bool ssl)
        : acceptor_(ios_)
    {
        try
        {
            acceptor_.open(boost::asio::ip::tcp::v6());
            acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port));
            acceptor_.listen();
        }
        catch(const boost::system::system_error &e)
        {
            if(e.code() == make_error_code(boost::asio::error::address_family_not_supported)) // no ipv6 capability
            {
                acceptor_.open(boost::asio::ip::tcp::v4());
                acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
                acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
                acceptor_.listen();
            }
            else
            {
                throw;
            }
        }

        if(ssl)
        {
            auto &conf = Configure::instance();

            ctx_ = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
            ctx_->set_password_callback(&HttpServer::handleGetPassword);

            std::vector<uint8_t> data;

            if(conf.exists("ssl_certificate_chain_file"))
            {
                data = readFileIntoVector(boost::filesystem::wpath(decodeUtf8(conf.get("ssl_certificate_chain_file"))));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_chain_file configure"));
                ctx_->use_certificate_chain(boost::asio::buffer(data));
            }
            else
            {
                data = readFileIntoVector(boost::filesystem::wpath(decodeUtf8(conf.get("ssl_certificate_file"))));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_file configure"));
                ctx_->use_certificate(boost::asio::buffer(data), boost::asio::ssl::context::pem);
            }
            data = readFileIntoVector(boost::filesystem::wpath(decodeUtf8(conf.get("ssl_private_key_file"))));
            if(data.empty())
                throw(std::runtime_error("invalid ssl_private_key_file configure"));
            ctx_->use_private_key(boost::asio::buffer(data), boost::asio::ssl::context::pem);

            acceptNextSsl();
        }
        else
        {
            acceptNext();
        }
    }

    void HttpServer::acceptNext()
    {
        nextConn_.reset(new HttpConnection(*this, false));
        acceptor_.async_accept(static_cast<BufferedWriterSocket<TcpSocket> *>(nextConn_->socket_.get())->socket(),
            boost::bind(&HttpServer::handleAccept, this, boost::asio::placeholders::error));
    }

    void HttpServer::acceptNextSsl()
    {
        nextConn_.reset(new HttpConnection(*this, true));
        acceptor_.async_accept(static_cast<BufferedWriterSocket<TcpSslSocket> *>(nextConn_->socket_.get())->socket(),
            boost::bind(&HttpServer::handleAcceptSsl, this, boost::asio::placeholders::error));
    }

    void HttpServer::handleAccept(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->read();
        }
        acceptNext();
    }

    void HttpServer::handleAcceptSsl(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->startSsl();
        }
        acceptNextSsl();
    }
}
