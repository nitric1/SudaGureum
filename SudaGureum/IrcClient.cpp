#include "Common.h"

#include "IrcClient.h"

namespace SudaGureum
{
    namespace
    {
        std::string encodeMessage(const IrcMessage &message)
        {
            std::string line;

            if(!message.prefix_.empty())
            {
                line = ":";
                line += message.prefix_;
                line += " ";
            }

            line += message.command_;

            if(!message.params_.empty())
            {
                for(auto it = message.params_.begin(); it != message.params_.end() - 1; ++ it)
                {
                    line += " ";
                    line += *it;
                }

                line += " :";
                line += message.params_.back();
            }

            return line;
        }
    }

    IrcClient::IrcClient(boost::asio::io_service &ios, IrcClientPool &pool)
        : pool_(pool)
        , ios_(ios)
        , socket_(ios)
        , inWrite_(false)
        , connectBeginning_(false)
        , currentNicknameIndex_(0)
        , quitReady_(false)
        , clearMe_(false)
    {
    }

    void IrcClient::connect(const std::string &addr, uint16_t port, const std::vector<std::string> &nicknames)
    {
        if(nicknames.empty() || nicknames[0].empty())
        {
            return;
        }

        nicknameCandidates_ = nicknames;
        currentNicknameIndex_ = 0;

        boost::asio::ip::tcp::resolver resolver(ios_);
        boost::asio::ip::tcp::resolver::query query(addr, boost::lexical_cast<std::string>(port));
        auto endpointIt = resolver.resolve(query);

        boost::asio::async_connect(socket_, endpointIt,
            [this](const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator resolverIt)
            {
                if(!ec)
                {
                    connectBeginning_ = true;
                    sendMessage(IrcMessage("USER", boost::assign::list_of(nicknameCandidates_[0])("0")("*")(nicknameCandidates_[0])));
                    sendMessage(IrcMessage("NICK", boost::assign::list_of(nicknameCandidates_[0])));
                    read();
                }
                else
                {
                    // callback
                }
            });
    }

    void IrcClient::tryNextNickname()
    {
        ++ currentNicknameIndex_;
        if(nicknameCandidates_.size() <= currentNicknameIndex_)
        {
            close();
            return;
        }

        sendMessage(IrcMessage("NICK", boost::assign::list_of(nicknameCandidates_[currentNicknameIndex_])));
    }

    void IrcClient::read()
    {
        socket_.async_read_some(
            boost::asio::buffer(bufferToRead_),
            boost::bind(
                std::mem_fn(&IrcClient::handleRead),
                shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred
            )
        );
    }

    void IrcClient::sendMessage(const IrcMessage &message)
    {
        {
            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            bufferToWrite_.push_back(encodeMessage(message) + "\r\n");
            std::cout << bufferToWrite_.back();
        }
        write();
    }

    void IrcClient::write(bool force)
    {
        if(inWrite_.exchange(true) && !force)
        {
            return;
        }

        std::string message;
        {
            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            if(!bufferToWrite_.empty())
            {
                message = bufferToWrite_.front();
                bufferToWrite_.pop_front();
            }
        }

        if(!message.empty())
        {
            std::shared_ptr<std::string> messagePtr(new std::string(message));

            boost::asio::async_write(
                socket_,
                boost::asio::buffer(*messagePtr, messagePtr->size()),
                boost::bind(
                    std::mem_fn(&IrcClient::handleWrite),
                    shared_from_this(),
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred,
                    messagePtr
                )
            );
        }
        else
        {
            inWrite_ = false;
        }
    }

    void IrcClient::close(bool clearMe)
    {
        quitReady_ = true;
        clearMe_ = clearMe;
        sendMessage(IrcMessage("QUIT", boost::assign::list_of("Bye!"))); // TODO: timeout required
    }

    void IrcClient::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            //
        }

        parser_.parse(std::string(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&IrcClient::procMessage, this, std::placeholders::_1));

        if(!quitReady_)
        {
            read();
        }
    }

    void IrcClient::handleWrite(const boost::system::error_code &ec, size_t bytesTransferred,
        const std::shared_ptr<std::string> &messagePtr)
    {
        if(ec || quitReady_)
        {
            inWrite_ = false;

            if(ec)
            {
                //
            }
        }

        write(true);
        inWrite_ = false;
    }

    void IrcClient::procMessage(const IrcMessage &message)
    {
        std::cout << encodeMessage(message) << std::endl;

        if(message.command_ == "PING")
        {
            sendMessage(IrcMessage("PONG", message.params_));
        }
        else if(message.command_ == "ERROR")
        {
            if(quitReady_) // gracefully
            {
                socket_.close();
                if(clearMe_)
                {
                    pool_.closed(shared_from_this());
                }
            }
        }
        else if(message.command_ == "PRIVMSG")
        {
        }
        else if(message.command_ == "NOTICE")
        {
        }
        else if(message.command_ == "432") // ERR_ERRONEUSNICKNAME
        {
            if(connectBeginning_)
            {
                tryNextNickname();
            }
        }
        else if(message.command_ == "433") // ERR_NICKNAMEINUSE
        {
            if(connectBeginning_)
            {
                tryNextNickname();
            }
        }
        else if(message.command_ == "436") // ERR_NICKCOLLISION
        {
            if(connectBeginning_)
            {
                tryNextNickname();
            }
        }
        else if(message.command_ == "437") // ERR_UNAVAILRESOURCE
        {
            if(connectBeginning_)
            {
                tryNextNickname();
            }
        }
        else if(message.command_ == "001") // RPL_WELCOME
        {
            connectBeginning_ = false;
            sendMessage(IrcMessage("JOIN", boost::assign::list_of("#zvuc")));
        }
    }

    IrcClientPool::IrcClientPool()
        : signals_(ios_)
    {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
#ifdef SIGQUIT
        signals_.add(SIGQUIT);
#endif

        signals_.async_wait([this](const boost::system::error_code &/*ec*/, int /*signo*/)
        {
            closeAll();
        });
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
    }

    void IrcClientPool::join()
    {
        for(auto &thread: threads_)
        {
            thread->join();
        }
    }

    std::weak_ptr<IrcClient> IrcClientPool::connect(const std::string &addr, uint16_t port, const std::vector<std::string> &nicknames)
    {
        std::shared_ptr<IrcClient> client(new IrcClient(ios_, *this));
        client->connect(addr, port, nicknames);
        clients_.push_back(client);
        return client;
    }

    void IrcClientPool::closeAll()
    {
        for(auto client: clients_)
        {
            client->close(false);
        }
        clients_.clear();
    }

    void IrcClientPool::closed(const std::shared_ptr<IrcClient> &client)
    {
        auto it = std::find(clients_.begin(), clients_.end(), client);
        if(it != clients_.end())
        {
            clients_.erase(it);
        }
    }

    void IrcClientPool::stop()
    {
        ios_.stop();
    }
}
