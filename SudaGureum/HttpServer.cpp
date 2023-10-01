#include "Common.h"

#include "HttpServer.h"

#include "AsioHelper.h"
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
        , bufferToRead_()
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
        Log::instance().info("HttpConnection[{}]: connection closed", static_cast<void *>(this));
    }

    void HttpConnection::sendRaw(std::vector<uint8_t> data)
    {
        socket_->asyncWrite(std::move(data),
            std::bind(
                std::mem_fn(&HttpConnection::handleWrite),
                shared_from_this(),
                StdAsioPlaceholders::error,
                StdAsioPlaceholders::bytesTransferred));
    }

    void HttpConnection::sendString(const std::string &str)
    {
        sendRaw(std::vector<uint8_t>(str.begin(), str.end()));
    }

    namespace
    {
        const char *httpStatusMessage(uint16_t status)
        {
#define _(code, message) case code: return (message)
            switch(status)
            {
                _(100, "Continue");
                _(101, "Switching Protocols");
                _(200, "OK");
                _(201, "Created");
                _(202, "Accepted");
                _(203, "Non-Authoritative Information");
                _(204, "No Content");
                _(205, "Reset Content");
                _(206, "Partial Content");
                _(300, "Multiple Choices");
                _(301, "Moved Permanently");
                _(302, "Found");
                _(303, "See Other");
                _(304, "Not Modified");
                _(305, "Use Proxy");
                _(307, "Temporary Redirect");
                _(400, "Bad Request");
                _(401, "Unauthorized");
                _(402, "Payment Required");
                _(403, "Forbidden");
                _(404, "Not Found");
                _(405, "Method Not Allowed");
                _(406, "Not Acceptable");
                _(407, "Proxy Authentication Required");
                _(408, "Request Time-out");
                _(409, "Conflict");
                _(410, "Gone");
                _(411, "Length Required");
                _(412, "Precondition Failed");
                _(413, "Request Entity Too Large");
                _(414, "Request-URI Too Large");
                _(415, "Unsupported Media Type");
                _(416, "Requested range not satisfiable");
                _(417, "Expectation Failed");
                _(500, "Internal Server Error");
                _(501, "Not Implemented");
                _(502, "Bad Gateway");
                _(503, "Service Unavailable");
                _(504, "Gateway Time-out");
                _(505, "HTTP Version not supported");
            default:
                //throw(std::invalid_argument(std::format("invalid status: {}", status)));
                //return nullptr;
                return "Unknown";
            }
#undef _
        }
    }

    void HttpConnection::sendHttpResponse(HttpResponse &&response)
    {
        std::string headerField = std::format(
            "HTTP/1.1 {} {}\r\n"
            "Server: SudaGureum HTTP Server\r\n"
            "Date: {}\r\n"
            "Content-Length: {}\r\n",
            response.status_, httpStatusMessage(response.status_),
            generateHttpDateTime(std::chrono::system_clock::now()),
            response.body_.size());
        for(const auto &header : response.headers_)
        {
            headerField += std::format("{}: {}\r\n", header.first, header.second);
        }
        headerField += "\r\n";

        sendString(headerField);
        sendRaw(std::move(response.body_));
    }

    void HttpConnection::sendHttpResponse(const HttpRequest &request, HttpResponse &&response)
    {
        bool useDeflate = false; // actually "zlib" format (deflate + zlib header)
        try
        {
            useDeflate = boost::ifind_first(request.headers_.at("Accept-Encoding"), "deflate");
        }
        catch(const std::out_of_range &)
        {
        }

        if(useDeflate)
        {
            unsigned long bufLen = static_cast<unsigned long>(response.body_.size() + 32);
            std::vector<uint8_t> buf(bufLen);
            compress2(buf.data(), &bufLen, response.body_.data(), static_cast<unsigned long>(response.body_.size()), Z_BEST_COMPRESSION);

            if(bufLen < response.body_.size())
            {
                response.headers_.insert_or_assign("Content-Encoding", "deflate");
                buf.resize(bufLen);
                response.body_ = std::move(buf);
            }
            else
            {
                useDeflate = false;
            }
        }

        response.headers_.insert({"Connection", "close"});

        std::string headerField = std::format(
            "HTTP/{} {} {}\r\n"
            "Server: SudaGureum HTTP Server\r\n"
            "Date: {}\r\n"
            "Content-Length: {}\r\n",
            request.http11_ ? "1.1" : "1.0", response.status_, httpStatusMessage(response.status_),
            generateHttpDateTime(std::chrono::system_clock::now()),
            response.body_.size());
        for(const auto &header : response.headers_)
        {
            headerField += std::format("{}: {}\r\n", header.first, header.second);
        }
        headerField += "\r\n";

        sendString(headerField);
        sendRaw(std::move(response.body_));
    }

    void HttpConnection::sendBadRequestResponse()
    {
        static const std::string Body = "Bad request";

        HttpResponse response;
        response.status_ = 403;
        response.headers_ =
        {
            {"Content-Type", "text/plain"},
        };
        response.body_.assign(Body.begin(), Body.end());

        sendHttpResponse(std::move(response));
    }

    void HttpConnection::startSsl()
    {
        socket_->asyncHandshakeAsServer(
            std::bind(
                std::mem_fn(&HttpConnection::handleHandshake),
                shared_from_this(),
                StdAsioPlaceholders::error));
    }

    void HttpConnection::read()
    {
        if(!socket_)
        {
            return;
        }

        socket_->asyncReadSome(
            asio::buffer(bufferToRead_),
            std::bind(
                std::mem_fn(&HttpConnection::handleRead),
                shared_from_this(),
                StdAsioPlaceholders::error,
                StdAsioPlaceholders::bytesTransferred));
    }

    void HttpConnection::close()
    {
        cancelKeepAliveTimeout();
        socket_->close();
    }

    void HttpConnection::setKeepAliveTimeout()
    {
        keepAliveTimer_.expires_from_now(std::chrono::seconds(
            Configure::instance().getAs("http_server_keep_alive_timeout_sec",
                DefaultConfigureValue::HttpServerKeepAliveTimeoutSec)));
        keepAliveTimer_.async_wait(std::bind(
            std::mem_fn(&HttpConnection::handleKeepAliveTimeout),
            shared_from_this(),
            StdAsioPlaceholders::error));
    }

    void HttpConnection::cancelKeepAliveTimeout()
    {
        std::error_code ec;
        keepAliveTimer_.cancel(ec);
    }

    void HttpConnection::handleHandshake(const std::error_code &ec)
    {
        if(ec)
        {
            Log::instance().warn("HttpConnection[{}]: handshake failed: {}", static_cast<void *>(this), ec.message());
            // socket automatically closed
            return;
        }

        read();
    }

    void HttpConnection::handleRead(const std::error_code &ec, size_t bytesTransferred)
    {
        if(!continueRead_)
        {
            return;
        }

        cancelKeepAliveTimeout();

        if(ec)
        {
            Log::instance().warn("HttpConnection[{}]: read failed: {}", static_cast<void *>(this), ec.message());
            close(); // implies forcely canceling write
            return;
        }

        std::vector<uint8_t> data(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred);

        auto res = parser_.parse(std::vector<uint8_t>(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&HttpConnection::procHttpRequest, this, std::placeholders::_1));
        if(!res.first)
        {
            Log::instance().warn("HttpConnection[{}]: read: invalid data received", static_cast<void *>(this));
            sendBadRequestResponse();
            // do not continue read -> socket closed after writing
            return;
        }
        else if(res.second > 0)
        {
            if(upgradeWebSocket_)
            {
                Log::instance().info("HttpConnection[{}]: read: upgrade to web socket", static_cast<void *>(this));
                data.erase(data.begin(), data.begin() + res.second);
                std::shared_ptr<WebSocketConnection> wsConn(new WebSocketConnection(server_, std::move(socket_), data));
                wsConn->handleRead(std::error_code(), data.size());
            }
            else
            {
                Log::instance().warn("HttpConnection[{}]: read: wrong upgrade header or not web socket", static_cast<void *>(this));
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

    void HttpConnection::handleWrite(const std::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            Log::instance().warn("HttpConnection[{}]: write failed: {}", static_cast<void *>(this), ec.message());
            close(); // implies forcely canceling read
            return;
        }
    }

    void HttpConnection::handleKeepAliveTimeout(const std::error_code &ec)
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

        Log::instance().info("HttpConnection[{}]: http request, target={} keepAlive=R{}/A{} curKAcnt={}", // R(requested), A(applied)
            static_cast<void *>(this), request.rawTarget_, request.keepAlive_, keepAlive, currentKeepAliveCount);

        if(request.upgrade_ && boost::iequals(request.headers_.at("Upgrade"), "websocket"))
        {
            if(!request.http11_)
            {
                return false;
            }

            // accept only WebSocket version 13
            if(request.headers_.at("Sec-WebSocket-Version").find("13") == std::string::npos)
            {
                return true;
            }

            // TODO: use HttpResponse?
            static constexpr std::string_view responseFormat =
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Date: {}\r\n"
                "Sec-WebSocket-Accept: {}\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n";

            std::string accept = request.headers_.at("Sec-WebSocket-Key") + WebSocketKeyConcatMagic;
            auto hash = hashSha1(std::vector<uint8_t>(accept.begin(), accept.end()));
            std::string acceptHashed = encodeBase64(std::vector<uint8_t>(hash.begin(), hash.end()));

            std::string response = std::format(responseFormat,
                generateHttpDateTime(std::chrono::system_clock::now()), acceptHashed);
            sendRaw(std::vector<uint8_t>(response.begin(), response.end()));

            upgradeWebSocket_ = true;
            return true;
        }

        HttpResponse response;

        try
        {
            auto handler = server_.getResourceProcessor(request.target_);
            response.status_ = 200;
            handler(*this, request, response);
        }
        catch(const std::out_of_range &)
        {
            static const std::string Content = "Not found";
            response.status_ = 404;
            response.headers_ = {
                {"Content-Type", "text/plain; charset=UTF-8"}
            };
            response.body_.assign(Content.begin(), Content.end());
        }

        if(keepAlive)
        {
            response.headers_.insert({"Connection", "keep-alive"});
            response.headers_.insert({
                "Keep-Alive",
                std::format("timeout={}, max={}",
                    Configure::instance().getAs("http_server_keep_alive_timeout_sec",
                        DefaultConfigureValue::HttpServerKeepAliveTimeoutSec),
                    currentKeepAliveCount)
            });
        }

        sendHttpResponse(request, std::move(response));

        if(!keepAlive)
        {
            continueRead_ = false;
            return false;
        }

        setKeepAliveTimeout();
        return true;
    }

    std::string HttpServer::handleGetPassword(size_t maxLength, asio::ssl::context::password_purpose purpose)
    {
        return Configure::instance().get("ssl_certificate_password").value_or("");
    }

    HttpServer::HttpServer(uint16_t port, bool ssl)
        : ssl_(ssl)
        , acceptor_(ios_)
    {
        try
        {
            acceptor_.open(asio::ip::tcp::v6());
            acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port));
            acceptor_.listen();
        }
        catch(const std::system_error &e)
        {
            if(e.code() == make_error_code(asio::error::address_family_not_supported)) // no ipv6 capability
            {
                acceptor_.open(asio::ip::tcp::v4());
                acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
                acceptor_.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
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

            ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);
            ctx_->set_password_callback(&HttpServer::handleGetPassword);

            std::vector<uint8_t> data;

            if(auto certChainFilePathStr = conf.get("ssl_certificate_chain_file"))
            {
                data = readFileIntoVector(std::filesystem::path(decodeUtf8(certChainFilePathStr.value())));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_chain_file configure"));
                ctx_->use_certificate_chain(asio::buffer(data));
            }
            else if(auto certFilePathStr = conf.get("ssl_certificate_file"))
            {
                data = readFileIntoVector(std::filesystem::path(decodeUtf8(certFilePathStr.value())));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_file configure"));
                ctx_->use_certificate(asio::buffer(data), asio::ssl::context::pem);
            }
            auto privateKeyFilePathStr = conf.get("ssl_certificate_file").value_or("");
            data = readFileIntoVector(std::filesystem::path(decodeUtf8(privateKeyFilePathStr)));
            if(data.empty())
                throw(std::runtime_error("invalid ssl_private_key_file configure"));
            ctx_->use_private_key(asio::buffer(data), asio::ssl::context::pem);

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
            std::bind(&HttpServer::handleAccept, this, StdAsioPlaceholders::error));
    }

    void HttpServer::acceptNextSsl()
    {
        nextConn_.reset(new HttpConnection(*this, true));
        acceptor_.async_accept(static_cast<BufferedWriterSocket<TcpSslSocket> *>(nextConn_->socket_.get())->socket(),
            std::bind(&HttpServer::handleAcceptSsl, this, StdAsioPlaceholders::error));
    }

    void HttpServer::handleAccept(const std::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->read();
        }
        acceptNext();
    }

    void HttpServer::handleAcceptSsl(const std::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->startSsl();
        }
        acceptNextSsl();
    }

    bool HttpServer::registerResourceProcessor(std::string path, ResourceProcessorFunc fn)
    {
        return processors_.insert({std::move(path), std::move(fn)}).second;
    }

    const HttpServer::ResourceProcessorFunc &HttpServer::getResourceProcessor(const std::string &path) const
    {
        return processors_.at(path);
    }
}
