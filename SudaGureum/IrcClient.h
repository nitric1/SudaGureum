#pragma once

#include "IrcParser.h"

namespace SudaGureum
{
    class IrcClient
    {
    private:
        IrcClient(boost::asio::io_service &ios);

    private:
        void connect(const std::string &addr, uint16_t port);
        void read();

    private:
        boost::asio::io_service &ios_;
        boost::asio::ip::tcp::socket socket_;
        IrcParser parser_;

        friend class IrcClientPool;
    };

    class IrcClientPool
    {
    public:
        IrcClientPool();

    public:
        void run(int numThreads);
        std::shared_ptr<IrcClient> connect(const std::string &addr, uint16_t port);

    private:
        boost::asio::io_service ios_;
        std::vector<std::shared_ptr<std::thread>> threads_;
    };
}
