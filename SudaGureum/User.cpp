#include "Common.h"

#include "User.h"

#include "DB.h"
#include "IrcClient.h"
#include "SCrypt.h"
#include "Session.h"

namespace SudaGureum
{
    User::User(std::string userId, IrcClientPool &ircClientPool, const UserServerInfoSet &servers)
        : userId_(std::move(userId))
        , ircClientPool_(ircClientPool)
        , archive_(userId_)
    {
        for(auto &info: servers)
        {
            auto inserted = servers_.emplace();
            if(inserted.second == false)
            {
                // TODO: error
            }

            auto modifier = [&servers, &info](Server &server)
            {
                *static_cast<UserServerInfo *>(&server) = info;
            };
            bool res = servers_.modify(inserted.first, modifier);
            assert(res);
        }
    }

    User::User(const UserEntry &userEntry, IrcClientPool &ircClientPool)
        : User(userEntry.userId_, ircClientPool, userEntry.servers_)
    {
    }

    const User::ServerSet &User::servers() const
    {
        return servers_;
    }

    void User::connectToServers()
    {
        auto modifier = [this](Server &info)
        {
            info.ircClient_ = ircClientPool_.connect(info.host_, info.port_, info.encoding_,
                info.nicknames_, info.ssl_,
                std::bind(&User::onIrcClientCreate, this, std::placeholders::_1));
        };

        for(auto it = servers_.begin(); it != servers_.end(); ++ it)
        {
            auto res = servers_.modify(it, modifier);
            assert(res);
        }
    }

    const User::Server *User::serverInfo(IrcClient *ircClient)
    {
        for(auto &info: servers_)
        {
            if(info.ircClient_.lock().get() == ircClient)
            {
                return &info;
            }
        }
        return nullptr;
    }

    const std::string &User::userId() const
    {
        return userId_;
    }

    Archive &User::archive()
    {
        return archive_;
    }

    void User::onIrcClientCreate(IrcClient &ircClient)
    {
        auto bindEvent = [this](auto fn)
        {
            return std::bind(fn, shared_from_this(), std::placeholders::_1);
        };

        ircClient.onConnect += bindEvent(&User::onIrcClientConnect);
        ircClient.onServerMessage += bindEvent(&User::onIrcClientServerMessage);
        ircClient.onJoinChannel += bindEvent(&User::onIrcClientJoinChannel);
        ircClient.onPartChannel += bindEvent(&User::onIrcClientPartChannel);
        ircClient.onChannelMessage += bindEvent(&User::onIrcClientChannelMessage);
        ircClient.onChannelNotice += bindEvent(&User::onIrcClientChannelNotice);
        ircClient.onPersonalMessage += bindEvent(&User::onIrcClientPersonalMessage);
    }

    void User::onIrcClientConnect(std::weak_ptr<IrcClient> ircClient)
    {
        auto ircClientLock = ircClient.lock();
        if(!ircClientLock)
        {
            return;
        }

        ircClientLock->mode("+x");

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        for(auto &channel: info->channels_)
        {
            ircClientLock->join(channel);
        }
    }

    // Find client connection from WebSocketServer
    void User::onIrcClientServerMessage(const IrcClient::ServerMessageArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            {},
            LogLine::SERVERMSG,
            args.text_,
        };

        archive_.insert(info->name_, "", logLine);
    }

    void User::onIrcClientJoinChannel(const IrcClient::JoinChannelArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            args.nickname_,
            LogLine::JOIN,
            {},
        };

        archive_.insert(info->name_, args.channel_, logLine);
    }

    void User::onIrcClientPartChannel(const IrcClient::PartChannelArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            args.nickname_,
            LogLine::PART,
            {},
        };

        archive_.insert(info->name_, args.channel_, logLine);
    }

    void User::onIrcClientChannelMessage(const IrcClient::ChannelMessageArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            args.nickname_,
            LogLine::PRIVMSG,
            args.message_,
        };

        archive_.insert(info->name_, args.channel_, logLine);
    }

    void User::onIrcClientChannelNotice(const IrcClient::ChannelMessageArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            args.nickname_,
            LogLine::NOTICE,
            args.message_,
        };

        if(args.nickname_.empty()) // server notice
        {
            archive_.insert(info->name_, {}, logLine);
        }
        else
        {
            archive_.insert(info->name_, args.channel_, logLine);
        }
    }

    void User::onIrcClientPersonalMessage(const IrcClient::PersonalMessageArgs &args)
    {
        auto ircClientLock = args.ircClient_.lock();
        if(!ircClientLock)
        {
            return;
        }

        auto info = serverInfo(ircClientLock.get());
        if(info == nullptr)
        {
            return;
        }

        LogLine logLine =
        {
            std::chrono::system_clock::now(),
            args.nickname_,
            LogLine::PRIVMSG,
            args.message_,
        };

        archive_.insert(info->name_, args.nickname_, logLine);
    }

    UserContext::UserContext()
    {
    }

    bool UserContext::authorize(std::string userId, const std::string &password)
    {
        if(!Users::instance().checkPassword(userId, password))
        {
            return false;
        }

        const UserSession &session = UserSessions::instance().alloc(userId);
        userId_ = std::move(userId);
        sessionKey_ = session.sessionKey_;
        return true;
    }

    bool UserContext::authorize(std::string sessionKey)
    {
        const UserSession *session = UserSessions::instance().get(sessionKey);
        if(!session)
        {
            return false;
        }

        userId_ = session->userId_;
        sessionKey_ = std::move(sessionKey);
        return true;
    }

    bool UserContext::authorized() const
    {
        return !userId_.empty() && !sessionKey_.empty();
    }

    const std::string &UserContext::userId() const
    {
        return userId_;
    }

    const std::string &UserContext::sessionKey() const
    {
        return sessionKey_;
    }

    void Users::load(IrcClientPool &ircClientPool)
    {
        // TODO: temporary; load from db

        UserServerInfo info = {"Ozinger", "irc.ozinger.org", 16666, "UTF-8", {"SudaGureum1", "SudaGureum2"}, true, {"#HNO3"}};
        std::shared_ptr<User> user(new User(
            "SudaGureum",
            ircClientPool,
            {info}));
        user->connectToServers();

        userMap_.emplace("SudaGureum", std::move(user));
    }

    std::shared_ptr<User> Users::user(const std::string &userId) const
    {
        auto it = userMap_.find(userId);
        if(it == userMap_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    bool Users::checkPassword(const std::string &userId, const std::string &password) const
    {
        std::string passwordHash = UserDB::instance().fetchPasswordHash(userId);
        return Scrypt::check(password, passwordHash);
    }
}
