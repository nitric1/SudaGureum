#include "Common.h"

#include "WebSocketServer.h"

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

    WebSocketConnection::WebSocketConnection(WebSocketServer &server)
        : server_(server)
        , ios_(server.ios_)
        , socket_(ios_)
        , closeReady_(false)
        , clearMe_(false)
    {
    }

    void WebSocketConnection::sendRaw(const std::vector<uint8_t> &data)
    {

    }

    void WebSocketConnection::sendMessage(const WebSocketMessage &message)
    {
    }

    void WebSocketConnection::read()
    {
        socket_.async_read_some(
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
            std::shared_ptr<std::vector<uint8_t>> framePtr(new std::vector<uint8_t>(std::move(frame)));

            boost::asio::async_write(
                socket_,
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

    void WebSocketConnection::close(bool clearMe)
    {
        closeReady_ = true;
        clearMe_ = clearMe;
        // TODO: send frame & timeout
    }

    void WebSocketConnection::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            std::cerr << ec.message() << std::endl;
            socket_->close();
            return;
        }

        parser_.parse(std::vector<uint8_t>(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&WebSocketConnection::procMessage, this, std::placeholders::_1));

        // if(!closeReady_)
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
                socket_->close();
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
    }

    WebSocketServer::WebSocketServer(uint16_t port)
        : acceptor_(ios_)
    {
        // TODO: IPv6

        acceptor_.open(boost::asio::ip::tcp::v4());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
        acceptor_.listen();

        acceptNext();
    }

    void WebSocketServer::acceptNext()
    {
        nextConn_.reset(new WebSocketConnection(*this));
        acceptor_.async_accept(nextConn_->socket_,
            boost::bind(&WebSocketServer::handleAccept, this, boost::asio::placeholders::error));
    }

    void WebSocketServer::handleAccept(const boost::system::error_code &ec)
    {
        acceptNext();
    }
}
