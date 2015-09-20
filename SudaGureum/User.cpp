#include "Common.h"

#include "User.h"

#include "IrcClient.h"

namespace SudaGureum
{
    User::User(std::string id, IrcClientPool &ircClientPool, const std::unordered_map<std::string, UserServerInfo> &servers)
        : id_(std::move(id))
        , ircClientPool_(ircClientPool)
    {
        for(auto &pair : servers)
        {
            auto res = servers_.emplace(std::piecewise_construct, std::forward_as_tuple(pair.first), std::forward_as_tuple());
            if(res.second == false)
            {
                // TODO: cannot occur
            }

            auto it = res.first;
            static_cast<UserServerInfo &>(it->second) = pair.second;
        }
    }

    const std::unordered_map<std::string, User::Server> &User::servers() const
    {
        return servers_;
    }

    void User::connectToServers()
    {
        for(auto &pair: servers_)
        {
            pair.second.ircClient_ = ircClientPool_.connect(pair.second.host_, pair.second.port_, pair.second.encoding_,
                pair.second.nicknames_, pair.second.ssl_,
                std::bind(&User::onIrcClientCreate, this, std::placeholders::_1));
        }
    }

    User::Server *User::serverInfo(IrcClient *ircClient)
    {
        for(auto &pair: servers_)
        {
            if(pair.second.ircClient_.lock().get() == ircClient)
            {
                return &pair.second;
            }
        }
        return nullptr;
    }

    void User::onIrcClientCreate(IrcClient &ircClient)
    {
        ircClient.onConnect += std::bind(&User::onIrcClientConnect, shared_from_this(), std::placeholders::_1);
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

    void Users::load(IrcClientPool &ircClientPool)
    {
        // TODO: temporary; load from db

        UserServerInfo info = {"altirc.ozinger.org", 80, "UTF-8", {"SudaGureum1", "SudaGureum2"}, false, {"#HNO3"}};
        std::shared_ptr<User> user(new User(
            "SudaGureum",
            ircClientPool,
            {std::make_pair("Ozinger", info)}));
        user->connectToServers();

        userMap_.emplace("SudaGureum", std::move(user));
    }

    std::shared_ptr<User> Users::user(const std::string &id) const
    {
        auto it = userMap_.find(id);
        if(it == userMap_.end())
        {
            return nullptr;
        }
        return it->second;
    }
}
