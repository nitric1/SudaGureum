﻿#include "Common.h"

#include "IrcClient.h"

#include "AsioHelper.h"
#include "Configure.h"
#include "Default.h"
#include "Log.h"
#include "Socket.h"
#include "Utility.h"

namespace SudaGureum
{
    namespace
    {
        void print(const std::wstring &wstr)
        {
#ifdef _WIN32
            size_t len = static_cast<size_t>(WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr));
            std::vector<char> buf(len);
            WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, buf.data(), static_cast<int>(len), nullptr, nullptr);
            std::cout << buf.data();
#else
            std::cout << boost::locale::conv::from_utf(wstr, std::locale());
#endif
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

    const IrcClient::NicknamePrefixMap IrcClient::DefaultNicknamePrefixMap =
    {
        {'~', 'q'}, {'&', 'a'}, {'@', 'o'}, {'%', 'h'}, {'+', 'v'}
    };

    std::string IrcClient::getNicknameFromPrefix(std::string prefix)
    {
        size_t userPrefixPos = prefix.find("!");
        if(userPrefixPos != std::string::npos)
        {
            return std::move(prefix.erase(userPrefixPos, std::string::npos));
        }

        size_t hostPrefixPos = prefix.find("@");
        if(hostPrefixPos != std::string::npos)
        {
            return std::move(prefix.erase(hostPrefixPos, std::string::npos));
        }

        return prefix;
    }

    IrcClient::IrcClient(IrcClientPool &pool, size_t connectionId)
        : pool_(pool)
        , connectionId_(connectionId)
        , ios_(pool.ios_)
        , bufferToRead_()
        , inWrite_(false)
        , nicknamePrefixMap_(DefaultNicknamePrefixMap)
        , connectBeginning_(false)
        , currentNicknameIndex_(0)
        , quitReady_(false)
        , closeTimer_(ios_)
        , clearMe_(false)
    {
    }

    size_t IrcClient::connectionId() const
    {
        return connectionId_;
    }

    // self
    void IrcClient::nickname(const std::string &nickname)
    {
        nickname_ = nickname;

        sendMessage(IrcMessage("NICK", {nickname}));
    }

    void IrcClient::mode(const std::string &modifier)
    {
        sendMessage(IrcMessage("MODE", {nickname_, modifier}));
    }

    // channel-specific
    void IrcClient::join(const std::string &channel)
    {
        sendMessage(IrcMessage("JOIN", {channel}));
    }

    void IrcClient::join(const std::string &channel, const std::string &key)
    {
        sendMessage(IrcMessage("JOIN", {channel, key}));
    }

    void IrcClient::part(const std::string &channel, const std::string &message)
    {
        sendMessage(IrcMessage("PART", {channel, message}));
    }

    void IrcClient::mode(const std::string &channel, const std::string &nickname, const std::string &modifier)
    {
        sendMessage(IrcMessage("MODE", {channel, nickname, modifier}));
    }

    void IrcClient::privmsg(const std::string &target, const std::string &message)
    {
        sendMessage(IrcMessage("PRIVMSG", {target, message}));
    }

    void IrcClient::connect(const std::string &host, uint16_t port, std::string encoding,
        std::vector<std::string> nicknames, bool ssl)
    {
        if(nicknames.empty() || nicknames[0].empty())
        {
            return;
        }

        encoding_ = std::move(encoding);
        nicknameCandidates_ = std::move(nicknames);
        currentNicknameIndex_ = 0;

        asio::ip::tcp::resolver resolver(ios_);
        asio::ip::tcp::resolver::query query(host, fmt::format_int(port).str());
        auto endpointIt = resolver.resolve(query);

        if(ssl)
            socket_ = std::make_shared<TcpSslSocket>(ios_);
        else
            socket_ = std::make_shared<TcpSocket>(ios_);
        socket_->asyncConnect(endpointIt,
            [&](const std::error_code &ec, asio::ip::tcp::resolver::iterator resolverIt)
            {
                if(ec)
                {
                    Log::instance().warn("IrcClient[{}]: connect failed: {}", static_cast<void *>(this), ec.message());
                    // TODO: retry
                }
                else
                {
                    connectBeginning_ = true;
                    sendMessage(IrcMessage("USER", {nicknameCandidates_[0], "0", "*", nicknameCandidates_[0]}));
                    nickname(nicknameCandidates_[0]);
                    read();
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
        socket_->asyncReadSome(
            asio::buffer(bufferToRead_),
            std::bind(
                std::mem_fn(&IrcClient::handleRead),
                shared_from_this(),
                StdAsioPlaceholders::error,
                StdAsioPlaceholders::bytesTransferred
            )
        );
    }

    void IrcClient::sendMessage(const IrcMessage &message)
    {
        {
            std::string encoded = encodeMessage(message);
            Log::instance().info("{}>>> {}", nickname_, encoded);

            std::lock_guard<std::mutex> lock(bufferWriteLock_);
            bufferToWrite_.push_back(std::move(encoded) + "\r\n");
            // print(decodeUtf8(nickname_ + ">>> " + bufferToWrite_.back()));
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
            auto messagePtr = std::make_shared<std::string>(std::move(message));

            socket_->asyncWrite(
                asio::buffer(*messagePtr, messagePtr->size()),
                std::bind(
                    std::mem_fn(&IrcClient::handleWrite),
                    shared_from_this(),
                    StdAsioPlaceholders::error,
                    StdAsioPlaceholders::bytesTransferred,
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
        sendMessage(IrcMessage("QUIT", {"Bye!"}));
        closeTimer_.expires_from_now(std::chrono::seconds(
            Configure::instance().getAs("irc_client_close_timeout_sec", DefaultConfigureValue::IrcClientCloseTimeoutSec)
        ));
        closeTimer_.async_wait(std::bind(
            std::mem_fn(&IrcClient::handleCloseTimeout),
            shared_from_this(),
            StdAsioPlaceholders::error
        ));
    }

    void IrcClient::forceClose()
    {
        socket_->close();
        pool_.closed(shared_from_this());
    }

    bool IrcClient::isMyPrefix(const std::string &prefix) const
    {
        return nickname_ == getNicknameFromPrefix(prefix);
    }

    bool IrcClient::isChannel(const std::string &str) const
    {
        if(str.empty())
            return false;
        return std::find(channelTypes_.begin(), channelTypes_.end(), str[0]) != channelTypes_.end();
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

    void IrcClient::handleRead(const std::error_code &ec, size_t bytesTransferred)
    {
        if(ec)
        {
            if(!quitReady_)
            {
                Log::instance().warn("IrcClient[{}]: read failed: {}", static_cast<void *>(this), ec.message());
                forceClose();
            }
            return;
        }

        if(!parser_.parse(std::string(bufferToRead_.begin(), bufferToRead_.begin() + bytesTransferred),
            std::bind(&IrcClient::procMessage, this, std::placeholders::_1)))
        {
            Log::instance().warn("IrcClient[{}]: invalid message received", static_cast<void *>(this));
            forceClose();
            return;
        }

        // if(!quitReady_)
        {
            read();
        }
    }

    void IrcClient::handleWrite(const std::error_code &ec, size_t bytesTransferred,
        const std::shared_ptr<std::string> &messagePtr)
    {
        inWrite_ = false;

        if(ec)
        {
            if(!quitReady_)
            {
                Log::instance().warn("IrcClient[{}]: write failed: {}", static_cast<void *>(this), ec.message());
                forceClose();
            }
            return;
        }

        write();
    }

    void IrcClient::handleCloseTimeout(const std::error_code &ec)
    {
        if(!ec)
        {
            // TODO: error message (close timeout)
            forceClose();
        }
    }

    void IrcClient::procMessage(const IrcMessage &message)
    {
        // TODO: assertions (error and close) or fail-safe process

        //print(decodeUtf8(nickname_ + "<<< " + encodeMessage(message)) + L"\r\n");
        Log::instance().info("{}<<< {}", nickname_, encodeMessage(message));

        // Dictionary order; letters first.

        if(message.command_ == "ERROR")
        {
            if(quitReady_) // graceful quit
            {
                socket_->close();
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
                    std::string nickname = getNicknameFromPrefix(message.prefix_);
                    it->second.participants_.insert(Participant(nickname));
                    onJoinChannel(JoinChannelArgs{shared_from_this(), channel, nickname});
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

                        if(boost::logic::indeterminate(operation)) // a +/- sign required for server response
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

                            // TODO: implement below modes
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
            // TODO: can send notices personally?
            onChannelNotice(ChannelMessageArgs{shared_from_this(),
                message.params_[0], getNicknameFromPrefix(message.prefix_), message.params_[1]});
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
            std::string channel = message.params_[0];
            if(channel == nickname_)
            {
                onPersonalMessage(PersonalMessageArgs{shared_from_this(),
                    getNicknameFromPrefix(message.prefix_), message.params_[1]});
            }
            else
            {
                onChannelMessage(ChannelMessageArgs{shared_from_this(),
                    message.params_[0], getNicknameFromPrefix(message.prefix_), message.params_[1]});
            }
        }
        else if(message.command_ == "001") // RPL_WELCOME
        {
            connectBeginning_ = false;

            onConnect(shared_from_this());
            onServerMessage(ServerMessageArgs{shared_from_this(), message.command_, message.params_[0]});
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
                    boost::algorithm::split(supportedModes, value, boost::algorithm::is_any_of(","));
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
                it->second.topicSetTime_ = std::chrono::system_clock::now();
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
                it->second.topicSetTime_ = std::chrono::system_clock::from_time_t(
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
            // join complete; you can see channel now
        }
        else if(message.command_ == "432") // ERR_ERRONEOUSNICKNAME
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
        , nextConnectionId_(1)
    {
        signals_.add(SIGINT);
        signals_.add(SIGTERM);
#ifdef SIGQUIT
        signals_.add(SIGQUIT);
#endif

        signals_.async_wait([this](const std::error_code &/*ec*/, int /*signo*/)
        {
            closeAll();
        });
    }

    std::weak_ptr<IrcClient> IrcClientPool::connect(const std::string &host, uint16_t port, std::string encoding,
        std::vector<std::string> nicknames, bool ssl, std::function<void (IrcClient &)> constructCb)
    {
        std::shared_ptr<IrcClient> client(new IrcClient(*this, nextConnectionId_ ++));
        if(constructCb)
        {
            constructCb(*client);
        }

        client->connect(host, port, std::move(encoding), std::move(nicknames), ssl);
        {
            std::lock_guard<std::mutex> lock(clientsLock_);
            clients_.emplace(client->connectionId_, client);
        }
        return client;
    }

    void IrcClientPool::closeAll()
    {
        std::lock_guard<std::mutex> lock(clientsLock_);
        for(auto &p: clients_)
        {
            p.second->close(false);
        }
        clients_.clear();
    }

    void IrcClientPool::closed(const std::shared_ptr<IrcClient> &client)
    {
        std::lock_guard<std::mutex> lock(clientsLock_);
        auto it = clients_.find(client->connectionId_);
        if(it != clients_.end())
        {
            clients_.erase(it);
        }
    }
}
