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

        static size_t participantModeFromPermission(char permission)
        {
            switch(permission)
            {
            case 'q':
                return IrcClient::Participant::Owner;
            case 'a':
                return IrcClient::Participant::Admin;
            case 'o':
                return IrcClient::Participant::Op;
            case 'h':
                return IrcClient::Participant::HalfOp;
            case 'v':
                return IrcClient::Participant::Voice;
            }
            return std::numeric_limits<size_t>::max();
        }
    }

    const IrcClient::NicknamePrefixMap IrcClient::DefaultNicknamePrefixMap = IrcClient::makeDefaultNicknamePrefixMap();

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

    IrcClient::NicknamePrefixMap IrcClient::makeDefaultNicknamePrefixMap()
    {
        return boost::assign::map_list_of('~', 'q')('&', 'a')('@', 'o')('%', 'h')('+', 'v');
    }

    IrcClient::IrcClient(IrcClientPool &pool)
        : pool_(pool)
        , ios_(pool.ios_)
        , socket_(ios_)
        , inWrite_(false)
        , nicknamePrefixMap_(DefaultNicknamePrefixMap)
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

    void IrcClient::connect(const std::string &addr, uint16_t port, const std::string &encoding,
        const std::vector<std::string> &nicknames)
    {
        if(nicknames.empty() || nicknames[0].empty())
        {
            return;
        }

        encoding_ = encoding;
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

    void IrcClient::write()
    {
        std::lock_guard<std::mutex> lock(writeLock_);

        if(inWrite_.exchange(true))
        {
            return;
        }

        std::string message;
        {
            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            if(!bufferToWrite_.empty())
            {
                message = std::move(bufferToWrite_.front());
                bufferToWrite_.pop_front();
            }
        }

        if(!message.empty())
        {
            std::shared_ptr<std::string> messagePtr(new std::string(std::move(message)));

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

    bool IrcClient::isMyPrefix(const std::string &prefix) const
    {
        return nickname_ == getNicknameFromPrefix(prefix);
    }

    IrcClient::Participant IrcClient::parseParticipant(const std::string &nicknameWithPrefix) const
    {
        Participant participant;
        if(nicknameWithPrefix.empty())
        {
            return participant;
        }

        auto it = nicknamePrefixMap_.find(nicknameWithPrefix[0]);
        if(it != nicknamePrefixMap_.end())
        {
            participant.modes_.set(participantModeFromPermission(it->second));
            participant.nickname_ = nicknameWithPrefix.substr(1);
        }
        else
        {
            participant.nickname_ = nicknameWithPrefix;
        }
        return participant;
    }

    void IrcClient::handleRead(const boost::system::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            // TODO: error log
            close();
            return;
        }

        parser_.parse(std::string(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&IrcClient::procMessage, this, std::placeholders::_1));

        // if(!quitReady_)
        {
            read();
        }
    }

    void IrcClient::handleWrite(const boost::system::error_code &ec, size_t bytesTransferred,
        const std::shared_ptr<std::string> &messagePtr)
    {
        inWrite_ = false;

        if(ec || quitReady_)
        {
            if(ec)
            {
                // TODO: error log
                close();
                return;
            }
        }

        write();
    }

    void IrcClient::procMessage(const IrcMessage &message)
    {
        // TODO: asserts(error and close) or fail-safe process

        print(Batang::decodeUTF8(nickname_ + "<<< " + encodeMessage(message)) + L"\r\n");

        // Dictionary order; letters first.

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

                // TODO: request channel modes
            }
            else
            {
                auto it = channels_.find(channel);
                if(it != channels_.end())
                {
                    it->second.participants_.insert(Participant(getNicknameFromPrefix(message.prefix_)));
                }
            }
        }
        else if(message.command_ == "MODE")
        {
            const std::string &to = message.params_.at(0);
            if(to == nickname_)
            {
            }
            else
            {
                auto it = channels_.find(to);
                if(it != channels_.end())
                {
                    const std::string &modifier = message.params_.at(1);

                    size_t nextParamIdx = 2;
                    auto nextParam = [&message, &nextParamIdx]() { return message.params_.at(nextParamIdx ++); };

                    boost::logic::tribool operation = boost::logic::indeterminate;
                    // XXX: need to follow CHANMODES and PREFIX?
                    for(char ch: modifier)
                    {
                        switch(ch)
                        {
                        case '+':
                            operation = true;
                            continue;

                        case '-':
                            operation = false;
                            continue;
                        }

                        if(boost::logic::indeterminate(operation)) // +- required for server response
                            continue;

                        switch(ch)
                        {
                        case 'O': // channel creator
                            break;

                        case 'q': // owner
                        case 'a': // admin
                        case 'o': // op
                        case 'h': // half-op
                        case 'v': // voice
                            if(!boost::logic::indeterminate(operation))
                            {
                                auto partIt = it->second.participants_.find(nextParam());
                                if(partIt != it->second.participants_.end())
                                {
                                    it->second.participants_.modify(partIt, [operation, ch](Participant &p)
                                    {
                                        if(operation)
                                        {
                                            p.modes_.set(participantModeFromPermission(ch));
                                        }
                                        else
                                        {
                                            p.modes_.reset(participantModeFromPermission(ch));
                                        }
                                    });
                                }
                            }
                            break;

                        case 'l': // limit
                            break;

                        case 'k': // key
                            break;

                        case 'b': // ban
                            break;

                        case 'e': // ban exception
                            break;

                        case 'I': // invitation mask
                            break;

                        default: // consume params
                            if(std::binary_search(channelModes_[0].begin(), channelModes_[0].end(), ch)) // mode A
                                nextParam();
                            else if(std::binary_search(channelModes_[1].begin(), channelModes_[1].end(), ch)) // mode B
                                nextParam();
                            else if(nicknamePrefixMap_.get<1>().find(ch) != nicknamePrefixMap_.get<1>().end()) // prefix; considered as mode B
                                nextParam();
                            else if(operation && std::binary_search(channelModes_[2].begin(), channelModes_[2].end(), ch)) // mode C
                                nextParam();
                            break;
                        }
                    }
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
            sendMessage(IrcMessage("MODE", boost::assign::list_of(nickname_)("+x")));
            sendMessage(IrcMessage("JOIN", boost::assign::list_of("#HNO3")));
        }
        else if(message.command_ == "005") // RPL_ISUPPORT
        {
            std::string name, value;
            for(auto it = ++ message.params_.begin(), end = -- message.params_.end(); it != end; ++ it)
            {
                size_t equalPos = it->find("=");
                if(equalPos != std::string::npos)
                {
                    name = it->substr(0, equalPos);
                    value = it->substr(equalPos + 1);
                    serverOptions_.emplace(name, value);
                }
                else
                {
                    value.clear();
                    serverOptions_.emplace(name, value);
                }

                if(name == "CHANTYPES")
                {
                    channelTypes_ = std::move(value);
                    std::sort(channelTypes_.begin(), channelTypes_.end());
                }
                else if(name == "CHANMODES")
                {
                    std::vector<std::string> supportedModes;
                    boost::algorithm::split(supportedModes, value, [](char ch) { return ch == ','; });
                    if(supportedModes.size() == 4)
                    {
                        for(size_t i = 0; i < 4; ++ i)
                        {
                            channelModes_[i] = std::move(supportedModes[i]);
                            std::sort(channelModes_[i].begin(), channelModes_[i].end());
                        }
                    }
                }
                else if(name == "PREFIX")
                {
                    static const boost::regex PrefixRegex("\\(([A-Za-z]+)\\)(.+)", boost::regex::extended);
                    boost::smatch matched;
                    if(boost::regex_match(value, matched, PrefixRegex))
                    {
                        if(matched[1].length() == matched[2].length())
                        {
                            nicknamePrefixMap_.clear();
                            for(auto valueIt = matched[1].first, keyIt = matched[2].first;
                                valueIt != matched[1].second && keyIt != matched[2].second;
                                ++ valueIt, ++ keyIt)
                            {
                                nicknamePrefixMap_.insert(CcPair(*keyIt, *valueIt));
                            }
                        }
                    }
                }
            }
        }
        else if(message.command_ == "331") // RPL_NOTOPIC
        {
            auto it = channels_.find(message.params_.at(1));
            if(it != channels_.end())
            {
                it->second.topic_ = "";
                it->second.topicSetter_ = "";
                it->second.topicSetTime_ = boost::posix_time::ptime();
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
        else if(message.command_ == "333") // RPL_TOPICSETTER (tentative)
        {
            auto it = channels_.find(message.params_.at(1));
            if(it != channels_.end())
            {
                it->second.topicSetter_ = message.params_.at(2);
                it->second.topicSetTime_ = boost::posix_time::from_time_t(
                    boost::lexical_cast<time_t>(message.params_.at(3)));
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

                std::vector<std::string> participantNicknames;
                boost::algorithm::split(participantNicknames, message.params_.at(3), boost::algorithm::is_space());
                for(std::string &nickname: participantNicknames)
                {
                    it->second.participants_.insert(parseParticipant(nickname));
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

    std::weak_ptr<IrcClient> IrcClientPool::connect(const std::string &addr, uint16_t port, const std::string &encoding,
        const std::vector<std::string> &nicknames)
    {
        std::shared_ptr<IrcClient> client(new IrcClient(*this));
        client->connect(addr, port, encoding, nicknames);
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
}
