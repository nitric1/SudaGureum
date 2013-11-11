#include "Common.h"

#include "WebSocketServer.h"

namespace SudaGureum
{
    WebSocketConnection::WebSocketConnection(WebSocketServer &server)
        : server_(server)
        , ios_(server.ios_)
        , socket_(ios_)
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
    }

    void WebSocketConnection::close()
    {
    }

    void WebSocketConnection::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            // TODO: error log
            close();
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
    }

    void WebSocketConnection::procMessage(const WebSocketMessage &message)
    {
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
