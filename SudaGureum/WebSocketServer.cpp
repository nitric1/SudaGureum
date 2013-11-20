#include "Common.h"

#include "WebSocketServer.h"

#include "Configure.h"
#include "EREndian.h"
#include "Utility.h"

namespace SudaGureum
{
    namespace
    {
        std::vector<uint8_t> encodeFrame(WebSocketFrameOpcode opcode, const std::vector<uint8_t> &data)
        {
            // Don't use fragmented frame

            std::vector<uint8_t> frame;
            frame.push_back(0x80 | static_cast<uint8_t>(opcode)); // final segment (1), reserved * 3 (000), opcode (XXXX)

            if(data.size() >= 0x10000)
            {
                frame.push_back(0x7F); // non-masked (0), 7-bit fragment size for 64 bits extended (1111111)
                uint64_t len = data.size();
                len = EREndian::N2B(len);
                uint8_t *lenByBytes = reinterpret_cast<uint8_t *>(len);
                frame.insert(frame.end(), lenByBytes, lenByBytes + sizeof(len));
            }
            else if(data.size() >= 0x7E)
            {
                frame.push_back(0x7E); // non-masked (0), 7-bit fragment size for 16 bits extended (1111110)
                uint16_t len = static_cast<uint16_t>(data.size());
                len = EREndian::N2B(len);
                uint8_t *lenByBytes = reinterpret_cast<uint8_t *>(len);
                frame.insert(frame.end(), lenByBytes, lenByBytes + sizeof(len));
            }
            else
            {
                frame.push_back(static_cast<uint8_t>(data.size())); // non-masked (0), 7-bit fragment size for < 0x7E (XXXXXXX)
            }

            frame.insert(frame.end(), data.begin(), data.end());

            return frame;
        }

        std::vector<uint8_t> encodeMessage(const WebSocketMessage &message)
        {
            return std::vector<uint8_t>();
        }
    }

    const std::string WebSocketConnection::KeyConcatMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    WebSocketConnection::WebSocketConnection(WebSocketServer &server, bool ssl)
        : server_(server)
        , ios_(server.ios_)
        , closeReady_(false)
        , closeReceived_(false)
    {
        if(ssl)
            socket_ = std::make_shared<TcpSslSocket>(ios_, server.ctx_);
        else
            socket_ = std::make_shared<TcpSocket>(ios_);
    }

    WebSocketConnection::~WebSocketConnection()
    {
        std::cerr << "Connection closed" << std::endl;
    }

    void WebSocketConnection::sendRaw(const std::vector<uint8_t> &data)
    {
        {
            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            bufferToWrite_.push_back(data);
        }
        write();
    }

    void WebSocketConnection::sendMessage(const WebSocketMessage &message)
    {
        // TODO: implement
    }

    void WebSocketConnection::startSsl()
    {
        socket_->asyncHandshakeAsServer(
            boost::bind(
                std::mem_fn(&WebSocketConnection::handleHandshake),
                shared_from_this(),
                boost::asio::placeholders::error
            )
        );
    }

    void WebSocketConnection::read()
    {
        socket_->asyncReadSome(
            boost::asio::buffer(bufferToRead_),
            boost::bind(
                std::mem_fn(&WebSocketConnection::handleRead),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred
            )
        );
    }

    void WebSocketConnection::write()
    {
        std::lock_guard<std::mutex> lock(writeLock_);

        if(inWrite_.exchange(true))
        {
            return;
        }

        std::vector<uint8_t> frame;
        {
            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            if(!bufferToWrite_.empty())
            {
                frame = std::move(bufferToWrite_.front());
                bufferToWrite_.pop_front();
            }
        }

        if(!frame.empty())
        {
            auto framePtr = std::make_shared<std::vector<uint8_t>>(std::move(frame));

            socket_->asyncWrite(
                boost::asio::buffer(*framePtr, framePtr->size()),
                boost::bind(
                    std::mem_fn(&WebSocketConnection::handleWrite),
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    framePtr
                )
            );
        }
        else
        {
            inWrite_ = false;
        }
    }

    void WebSocketConnection::close()
    {
        closeReady_ = true;
        sendRaw(encodeFrame(Close, std::vector<uint8_t>())); // TODO: timeout
    }

    void WebSocketConnection::handleHandshake(const boost::system::error_code &ec)
    {
        if(ec)
        {
            std::cerr << ec.message() << std::endl;
            // socket automatically closed
            return;
        }

        read();
    }

    void WebSocketConnection::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            std::cerr << ec.message() << std::endl;
            socket_->close(); // implies forcely canceling write
            return;
        }

        parser_.parse(std::vector<uint8_t>(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&WebSocketConnection::procMessage, this, std::placeholders::_1));

        if(!closeReceived_)
        {
            read();
        }
    }

    void WebSocketConnection::handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::vector<uint8_t>> &messagePtr)
    {
        inWrite_ = false;

        if(ec || closeReady_)
        {
            if(ec)
            {
                std::cerr << ec.message() << std::endl;
                socket_->close(); // implies forcely canceling read
                return;
            }
        }

        write();
    }

    void WebSocketConnection::procMessage(const WebSocketMessage &message)
    {
        if(message.command_ == "BadRequest")
        {
            static const std::string response =
                "HTTP/1.1 400 Bad Request\r\n"
                "Connection: close\r\n"
                "Content-Type: text/plain; charset=UTF-8\r\n"
                "\r\n"
                "The request is bad.";
            sendRaw(std::vector<uint8_t>(response.begin(), response.end()));
        }
        else if(message.command_ == "HandshakeRequest")
        {
            boost::format responseFormat(
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Connection: Upgrade\r\n"
                "Upgrade: websocket\r\n"
                "Sec-WebSocket-Accept: %1%\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");

            std::string accept = message.params_.find("Key")->second + KeyConcatMagic;
            auto hash = hashSha1(std::vector<uint8_t>(accept.begin(), accept.end()));
            std::string acceptHashed = encodeBase64(std::vector<uint8_t>(hash.begin(), hash.end()));

            std::string response = (responseFormat % acceptHashed).str();
            sendRaw(std::vector<uint8_t>(response.begin(), response.end()));
        }
        else if(message.command_ == "Ping")
        {
            sendRaw(encodeFrame(Pong, message.rawData_));
        }
        else if(message.command_ == "Close")
        {
            closeReceived_ = true;
            if(closeReady_) // already sent close frame
            {
                socket_->close();
            }
            else
            {
                close();
            }
        }
    }

    std::string WebSocketServer::handleGetPassword(size_t maxLength, boost::asio::ssl::context::password_purpose purpose)
    {
        return Configure::instance().get("ssl_certificate_password");
    }

    WebSocketServer::WebSocketServer(uint16_t port, bool ssl)
        : acceptor_(ios_)
    {
        // TODO: IPv6

        acceptor_.open(boost::asio::ip::tcp::v4());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
        acceptor_.listen();

        if(ssl)
        {
            auto conf = Configure::instance();

            ctx_ = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);
            ctx_->set_password_callback(&WebSocketServer::handleGetPassword);

            std::vector<uint8_t> data;

            if(conf.exists("ssl_certificate_chain_file"))
            {
                data = readFile(boost::filesystem::wpath(decodeUtf8(Configure::instance().get("ssl_certificate_chain_file"))));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_chain_file configure"));
                ctx_->use_certificate_chain(boost::asio::buffer(data));
            }
            else
            {
                data = readFile(boost::filesystem::wpath(decodeUtf8(Configure::instance().get("ssl_certificate_file"))));
                if(data.empty())
                    throw(std::runtime_error("invalid ssl_certificate_file configure"));
                ctx_->use_certificate(boost::asio::buffer(data), boost::asio::ssl::context::pem);
            }
            data = readFile(boost::filesystem::wpath(decodeUtf8(Configure::instance().get("ssl_private_key_file"))));
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

    void WebSocketServer::acceptNext()
    {
        nextConn_.reset(new WebSocketConnection(*this, false));
        acceptor_.async_accept(static_cast<TcpSocket *>(nextConn_->socket_.get())->socket(),
            boost::bind(&WebSocketServer::handleAccept, this, boost::asio::placeholders::error));
    }

    void WebSocketServer::acceptNextSsl()
    {
        nextConn_.reset(new WebSocketConnection(*this, true));
        acceptor_.async_accept(static_cast<TcpSslSocket *>(nextConn_->socket_.get())->socket(),
            boost::bind(&WebSocketServer::handleAcceptSsl, this, boost::asio::placeholders::error));
    }

    void WebSocketServer::handleAccept(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->read();
        }
        acceptNext();
    }

    void WebSocketServer::handleAcceptSsl(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            nextConn_->startSsl();
        }
        acceptNextSsl();
    }
}
