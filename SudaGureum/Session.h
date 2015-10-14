#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    struct UserSession
    {
        std::string sessionKey_;
        std::string userId_;
    };

    typedef boost::multi_index_container<
        UserSession,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::member<UserSession, std::string, &UserSession::sessionKey_>
            >
        >
    > UserSessionSet;

    class UserSessions : public Singleton<UserSessions>
    {
    private:
        std::string generateSessionKey(const std::string &userId);

    public:
        const UserSession &alloc(std::string userId);
        const UserSession *get(const std::string &sessionKey);

    private:
        std::mutex sessionsLock_;
        UserSessionSet sessions_;

        friend class Singleton<UserSessions>;
    };
}
