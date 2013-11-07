#include "Common.h"

#include "Batang/Utility.h"

#include "IrcClient.h"

namespace SudaGureum
{
    namespace
    {
        void print(const std::wstring &wstr)
        {
            size_t len = static_cast<size_t>(WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr));
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, buf.data(), static_cast<int>(len), nullptr, nullptr);
            std::cout << buf.data();
        }

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

    std::string IrcClient::getNicknameFromPrefix(const std::string &prefix)
    {
        size_t userPrefixPos = prefix.find("!");
        if(userPrefixPos != std::string::npos)
        {
            return prefix.substr(0, userPrefixPos);
        }

        size_t hostPrefixPos = prefix.find("@");
        if(hostPrefixPos != std::string::npos)
        {
            return prefix.substr(0, hostPrefixPos);
        }

        return prefix;
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

    void IrcClient::nickname(const std::string &nickname)
    {
        nickname_ = nickname;

        sendMessage(IrcMessage("NICK", boost::assign::list_of(nickname)));
    }

    void IrcClient::join(const std::string &channel)
    {
        sendMessage(IrcMessage("JOIN", boost::assign::list_of(channel)));
    }

    void IrcClient::join(const std::string &channel, const std::string &key)
    {
        sendMessage(IrcMessage("JOIN", boost::assign::list_of(channel)(key)));
    }

    void IrcClient::part(const std::string &channel, const std::string &message)
    {
        sendMessage(IrcMessage("PART", boost::assign::list_of(channel)(message)));
    }

    void IrcClient::privmsg(const std::string &channel, const std::string &message)
    {
        sendMessage(IrcMessage("PRIVMSG", boost::assign::list_of(channel)(message)));
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
                    nickname(nicknameCandidates_[0]);
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
            // XXX: or random nickname?
            return;
        }

        nickname(nicknameCandidates_[currentNicknameIndex_]);
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
            print(Batang::decodeUTF8(nickname_ + ">>> " + bufferToWrite_.back()));
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

    bool IrcClient::isMyPrefix(const std::string &prefix)
    {
        return nickname_ == getNicknameFromPrefix(prefix);
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
        // TODO: asserts(error and close) or fail-safe process

        print(Batang::decodeUTF8(nickname_ + "<<< " + encodeMessage(message)) + L"\r\n");

        // Dictionary order command; letters first.

        if(message.command_ == "ERROR")
        {
            if(quitReady_) // graceful quit
            {
                socket_.close();
                if(clearMe_)
                {
                    pool_.closed(shared_from_this());
                }
            }
        }
        else if(message.command_ == "JOIN")
        {
            const std::string &channel = message.params_.at(0);
            if(isMyPrefix(message.prefix_))
            {
                channels_.emplace(channel, Channel());
            }
            else
            {
                auto it = channels_.find(channel);
                if(it != channels_.end())
                {
                    it->second.participants_.insert(getNicknameFromPrefix(message.prefix_));
                }
            }
        }
        else if(message.command_ == "NOTICE")
        {
        }
        else if(message.command_ == "PART")
        {
            const std::string &channel = message.params_.at(0);
            if(isMyPrefix(message.prefix_))
            {
                channels_.erase(channel);
            }
            else
            {
                auto it = channels_.find(channel);
                if(it != channels_.end())
                {
                    it->second.participants_.erase(getNicknameFromPrefix(message.prefix_));
                }
            }
        }
        else if(message.command_ == "PING")
        {
            sendMessage(IrcMessage("PONG", message.params_));
        }
        else if(message.command_ == "PRIVMSG")
        {
        }
        else if(message.command_ == "001") // RPL_WELCOME
        {
            connectBeginning_ = false;
            sendMessage(IrcMessage("JOIN", boost::assign::list_of("#zvuc")));
        }
        else if(message.command_ == "331") // RPL_NOTOPIC
        {
            auto it = channels_.find(message.params_.at(1));
            if(it != channels_.end())
            {
                it->second.topic_ = "";
            }
        }
        else if(message.command_ == "332") // RPL_TOPIC
        {
            auto it = channels_.find(message.params_.at(1));
            if(it != channels_.end())
            {
                it->second.topic_ = message.params_.at(2);
            }
        }
        else if(message.command_ == "353") // RPL_NAMREPLY
        {
            auto it = channels_.find(message.params_.at(2));
            if(it != channels_.end())
            {
                char accessivity = message.params_.at(1)[0];
                switch(accessivity)
                {
                case Channel::Public:
                case Channel::Private:
                case Channel::Secret:
                    // OK
                    break;

                default:
                    return;
                }

                it->second.accessivity_ = accessivity;

                std::vector<std::string> participants;
                boost::algorithm::split(participants, message.params_.at(3), boost::algorithm::is_space());
                for(std::string &participant: participants)
                {
                    it->second.participants_.emplace(std::move(participant)); // TODO: split permission
                }
            }
        }
        else if(message.command_ == "366") // RPL_ENDOFNAMES
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
        {
            std::lock_guard<std::mutex> lock(clientsLock_);
            clients_.insert(client);
        }
        return client;
    }

    void IrcClientPool::closeAll()
    {
        std::lock_guard<std::mutex> lock(clientsLock_);
        for(auto client: clients_)
        {
            client->close(false);
        }
        clients_.clear();
    }

    void IrcClientPool::closed(const std::shared_ptr<IrcClient> &client)
    {
        std::lock_guard<std::mutex> lock(clientsLock_);
        auto it = clients_.find(client);
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
