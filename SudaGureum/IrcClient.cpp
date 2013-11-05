#include "Common.h"

#include "IrcClient.h"

namespace SudaGureum
{
    IrcClient::IrcClient(boost::asio::io_service &ios)
        : ios_(ios)
        , socket_(ios)
    {
    }

    void IrcClient::connect(const std::string &addr, uint16_t port)
    {
        boost::asio::ip::tcp::resolver resolver(ios_);
        boost::asio::ip::tcp::resolver::query query(addr, boost::lexical_cast<std::string>(port));
        auto endpointIt = resolver.resolve(query);

        boost::asio::async_connect(socket_, endpointIt,
            [this](boost::system::error_code ec, boost::asio::ip::tcp::resolver::iterator resolverIt)
            {
                if(!ec)
                {
                    read();
                }
                else
                {
                    // callback
                }
            });
    }

    void IrcClient::read()
    {
    }

    void IrcClientPool::run(int numThreads)
    {
        for(int i = 0; i < numThreads; ++ i)
        {
            std::shared_ptr<std::thread> thread(
                new std::thread(std::bind(
                    static_cast<size_t (boost::asio::io_service::*)()>(&boost::asio::io_service::run), &ios_)));
            threads_.push_back(thread);
        }

        for(auto &thread: threads_)
        {
            thread->join();
        }
    }

    std::shared_ptr<IrcClient> IrcClientPool::connect(const std::string &addr, uint16_t port)
    {
        std::shared_ptr<IrcClient> client(new IrcClient(ios_));
        client->connect(addr, port);
        return client;
    }
}
