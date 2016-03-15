#pragma once

#include "Archive.h"
#include "IrcClient.h"
#include "Singleton.h"
#include "Utility.h"

namespace SudaGureum
{
    struct UserServerInfo
    {
        std::string name_;
        std::string host_;
        uint16_t port_;
        std::string encoding_;
        std::vector<std::string> nicknames_;
        bool ssl_;
        std::vector<std::string> channels_;
    };

    typedef boost::multi_index_container<
        UserServerInfo,
        boost::multi_index::indexed_by<
            boost::multi_index::ordered_unique<
                boost::multi_index::member<UserServerInfo, std::string, &UserServerInfo::name_>,
                LessCaseInsensitive
            >
        >
    > UserServerInfoSet;

    class IrcClient;
    class IrcClientPool;

    struct UserEntry
    {
        std::string userId_;
        UserServerInfoSet servers_;
    };

    class User : public std::enable_shared_from_this<User>
    {
    public:
        struct Server : UserServerInfo
        {
            std::weak_ptr<IrcClient> ircClient_;
        };

        typedef boost::multi_index_container<
            Server,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_unique<
                    boost::multi_index::member<UserServerInfo, std::string, &Server::name_>,
                    LessCaseInsensitive
                >
            >
        > ServerSet;

    public:
        User(std::string userId, IrcClientPool &ircClientPool,
            const UserServerInfoSet &servers);
        User(const UserEntry &userEntry, IrcClientPool &ircClientPool);

    public:
        const ServerSet &servers() const;

    public:
        void connectToServers();
        const Server *serverInfo(IrcClient *ircClient);

    public:
        const std::string &userId() const;
        Archive &archive();

    private:
        void onIrcClientCreate(IrcClient &ircClient);
        void onIrcClientConnect(std::weak_ptr<IrcClient> ircClient);
        void onIrcClientServerMessage(const IrcClient::ServerMessageArgs &args);
        void onIrcClientJoinChannel(const IrcClient::JoinChannelArgs &args);
        void onIrcClientPartChannel(const IrcClient::PartChannelArgs &args);
        void onIrcClientChannelMessage(const IrcClient::ChannelMessageArgs &args);
        void onIrcClientChannelNotice(const IrcClient::ChannelMessageArgs &args);
        void onIrcClientPersonalMessage(const IrcClient::PersonalMessageArgs &args);

    private:
        std::string userId_;
        IrcClientPool &ircClientPool_;
        ServerSet servers_;
        Archive archive_;
    };

    class UserContext
    {
    private:
        UserContext(const UserContext &) = delete;
        UserContext &operator =(const UserContext &) = delete;

    public:
        UserContext();

    public:
        bool authorize(std::string userId, const std::string &password);
        bool authorize(std::string sessionKey);

    public:
        bool authorized() const;
        const std::string &userId() const;
        const std::string &sessionKey() const;

    private:
        std::string userId_;
        std::string sessionKey_;
    };

    class Users : public Singleton<Users>
    {
    private:
        ~Users() {}

    public:
        void load(IrcClientPool &ircClientPool);
        std::shared_ptr<User> user(const std::string &userId) const;
        bool checkPassword(const std::string &userId, const std::string &password) const;

    private:
        std::map<std::string, std::shared_ptr<User>> userMap_;

        friend class Singleton<Users>;
    };
}
