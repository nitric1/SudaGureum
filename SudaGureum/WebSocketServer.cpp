#include "Common.h"

#include "WebSocketServer.h"

#include "Configure.h"
#include "Default.h"
#include "HttpServer.h"
#include "Log.h"
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
                len = boost::endian::native_to_big(len);
                uint8_t *lenByBytes = reinterpret_cast<uint8_t *>(len);
                frame.insert(frame.end(), lenByBytes, lenByBytes + sizeof(len));
            }
            else if(data.size() >= 0x7E) // && data.size() < 0x10000
            {
                frame.push_back(0x7E); // non-masked (0), 7-bit fragment size for 16 bits extended (1111110)
                uint16_t len = static_cast<uint16_t>(data.size());
                len = boost::endian::native_to_big(len);
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
    }

    const std::string WebSocketConnection::KeyConcatMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    WebSocketConnection::WebSocketConnection(HttpServer &server, bool ssl)
        : server_(server)
        , ios_(server.ios_)
        , closeReady_(false)
        , closeTimer_(ios_)
        , closeReceived_(false)
    {
        if(ssl)
            socket_ = std::make_shared<BufferedWriterSocket<TcpSslSocket>>(ios_, server.ctx_);
        else
            socket_ = std::make_shared<BufferedWriterSocket<TcpSocket>>(ios_);
    }

    WebSocketConnection::WebSocketConnection(HttpServer &server,
        std::shared_ptr<BufferedWriterSocketBase> socket, const std::vector<uint8_t> &readBuffer)
        : server_(server)
        , ios_(server.ios_)
        , socket_(std::move(socket))
        , closeReady_(false)
        , closeTimer_(ios_)
        , closeReceived_(false)
    {
        if(readBuffer.size() > bufferToRead_.size())
        {
            throw(std::logic_error("readBuffer is too large"));
        }
        std::copy(readBuffer.begin(), readBuffer.end(), bufferToRead_.begin());
        // handleRead is called by HttpConnection because shared_from_this is not available here.
    }

    WebSocketConnection::~WebSocketConnection()
    {
        Log::instance().info("WebSocketConnection[{}]: connection closed", static_cast<void *>(this));
    }

    void WebSocketConnection::sendRaw(std::vector<uint8_t> data)
    {
        socket_->asyncWrite(std::move(data),
            boost::bind(
                std::mem_fn(&WebSocketConnection::handleWrite),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void WebSocketConnection::sendWebSocketResponse(const WebSocketResponse &response)
    {
        WebSocketFrameOpcode opcode = response.opcode();
        sendRaw(encodeFrame(opcode, response.rawData_));
    }

    void WebSocketConnection::sendSudaGureumResponse(SudaGureumResponse &&response)
    {
        response.responseDoc_.AddMember("_reqid", response.id_, response.responseDoc_.GetAllocator());
        response.responseDoc_.AddMember("success", response.success_, response.responseDoc_.GetAllocator());

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.responseDoc_.Accept(writer);

        const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer.GetString());

        WebSocketResponse wsResponse(WebSocketResponse::Command::Text,
            std::vector<uint8_t>(data, data + buffer.GetSize()));
        sendWebSocketResponse(wsResponse);
    }

    void WebSocketConnection::startSsl()
    {
        socket_->asyncHandshakeAsServer(
            boost::bind(
                std::mem_fn(&WebSocketConnection::handleHandshake),
                shared_from_this(),
                boost::asio::placeholders::error));
    }

    void WebSocketConnection::read()
    {
        socket_->asyncReadSome(
            boost::asio::buffer(bufferToRead_),
            boost::bind(
                std::mem_fn(&WebSocketConnection::handleRead),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

    void WebSocketConnection::close()
    {
        closeReady_ = true;
        sendWebSocketResponse(WebSocketResponse(WebSocketResponse::Command::Close));
        closeTimer_.expires_from_now(boost::posix_time::seconds(
            Configure::instance().getAs("websocket_server_close_timeout_sec",
                DefaultConfigureValue::WebSocketServerCloseTimeoutSec)));
        closeTimer_.async_wait(boost::bind(
            std::mem_fn(&WebSocketConnection::handleCloseTimeout),
            shared_from_this(),
            boost::asio::placeholders::error));
    }

    void WebSocketConnection::handleHandshake(const boost::system::error_code &ec)
    {
        if(ec)
        {
            Log::instance().warn("WebSocketConnection[{}]: handshake failed: {}", static_cast<void *>(this), ec.message());
            // socket automatically closed
            return;
        }

        read();
    }

    void WebSocketConnection::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            Log::instance().warn("WebSocketConnection[{}]: read failed: {}", static_cast<void *>(this), ec.message());
            if(!closeReady_)
            {
                socket_->close(); // implies forcely canceling write
            }
            return;
        }

        if(!parser_.parse(std::vector<uint8_t>(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&WebSocketConnection::procWebSocketRequest, this, std::placeholders::_1),
            std::bind(&WebSocketConnection::procSudaGureumRequest, this, std::placeholders::_1)))
        {
            Log::instance().warn("WebSocketConnection[{}]: read: invalid data received", static_cast<void *>(this));
            socket_->close();
            return;
        }

        if(!closeReceived_)
        {
            read();
        }
    }

    void WebSocketConnection::handleWrite(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            Log::instance().warn("WebSocketConnection[{}]: write failed: {}", static_cast<void *>(this), ec.message());
            if(!closeReady_)
            {
                socket_->close(); // implies forcely canceling read
            }
            return;
        }
    }

    void WebSocketConnection::handleCloseTimeout(const boost::system::error_code &ec)
    {
        if(!ec)
        {
            // TODO: error message (close timeout)
            socket_->close();
        }
    }

    void WebSocketConnection::procWebSocketRequest(const WebSocketRequest &request)
    {
        switch(request.command_)
        {
        case WebSocketRequest::Command::Ping:
            sendWebSocketResponse(WebSocketResponse(WebSocketResponse::Command::Pong, request.rawData_));
            break;

        case WebSocketRequest::Command::Close:
            closeReceived_ = true;
            if(closeReady_) // close frame is already sent
            {
                closeTimer_.cancel();
                socket_->close();
            }
            else
            {
                close();
            }
            break;
        }
    }

    void WebSocketConnection::procSudaGureumRequest(const SudaGureumRequest &request)
    {
        // TODO: implement
        if(request.method_ == "heartbeat")
        {
            rapidjson::Document seenEidsDoc;

            try
            {
                if(!seenEidsDoc.Parse<0>(request.params_.at("seenEids").c_str()).HasParseError())
                {
                    throw(RapidJson::Exception(""));
                    return;
                }

                // TODO: implement
            }
            catch(const RapidJson::Exception &)
            {
                SudaGureumResponse response(request, false);
                response.responseDoc_.AddMember("message", "invalid json", response.responseDoc_.GetAllocator());
                sendSudaGureumResponse(std::move(response));
            }
        }
    }
}
