#pragma once

namespace SudaGureum
{
    class WebSocketServer
    {
    public:
        WebSocketServer(uint16_t port);

    private:
        boost::asio::io_service ios_;
    };
}
