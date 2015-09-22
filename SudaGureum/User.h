#pragma once

#include "Archive.h"
#include "Singleton.h"

namespace SudaGureum
{
    struct UserServerInfo
    {
        std::string host_;
        uint16_t port_;
        std::string encoding_;
        std::vector<std::string> nicknames_;
        bool ssl_;

        std::vector<std::string> channels_;
    };

    class IrcClient;
    class IrcClientPool;

    class User : public std::enable_shared_from_this<User>
    {
    public:
        struct Server : UserServerInfo
        {
            std::weak_ptr<IrcClient> ircClient_;
        };

    public:
        User(std::string id, IrcClientPool &ircClientPool,
            const std::unordered_map<std::string, UserServerInfo> &servers);

    public:
        const std::unordered_map<std::string, Server> &servers() const;

    public:
        void connectToServers();
        Server *serverInfo(IrcClient *ircClient);

    public:
        const std::string &id() const;
        Archive &archive();

    private:
        void onIrcClientCreate(IrcClient &ircClient);
        void onIrcClientConnect(std::weak_ptr<IrcClient> ircClient);

    private:
        std::string id_;
        IrcClientPool &ircClientPool_;
        std::unordered_map<std::string, Server> servers_;
        Archive archive_;
    };

    class Users : public Singleton<Users>
    {
    private:
        ~Users() {}

    public:
        void load(IrcClientPool &ircClientPool);
        std::shared_ptr<User> user(const std::string &id) const;

    private:
        std::map<std::string, std::shared_ptr<User>> userMap_;

        friend class Singleton<Users>;
    };
}
