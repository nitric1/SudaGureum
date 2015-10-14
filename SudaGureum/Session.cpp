#include "Common.h"

#include "Session.h"

namespace SudaGureum
{
    std::string UserSessions::generateSessionKey(const std::string &userId)
    {
        static boost::uuids::random_generator generator;
        std::string key;
        do
        {
            key = boost::uuids::to_string(generator());
        }
        while(sessions_.find(key) != sessions_.end());
        return key;
    }

    const UserSession &UserSessions::alloc(std::string userId)
    {
        std::lock_guard<std::mutex> lock(sessionsLock_);
        UserSession session = {generateSessionKey(userId), std::move(userId)};
        auto res = sessions_.insert(session);
        if(!res.second)
        {
            // TODO: throw exception
        }
        return *res.first;
    }

    const UserSession *UserSessions::get(const std::string &sessionKey)
    {
        std::lock_guard<std::mutex> lock(sessionsLock_);
        auto it = sessions_.find(sessionKey);
        if(it == sessions_.end())
            return nullptr;
        return &*it;
    }
}
