#pragma once

#include "Archive.h"
#include "User.h"
#include "Singleton.h"

namespace SudaGureum
{
    class ArchiveDB : public Singleton<ArchiveDB>
    {
    private:
        static const std::string DBName;

    private:
        ArchiveDB();

    public:
        std::vector<LogLine> fetchLogs(const std::string &userId, const std::string &serverName, const std::string &channel,
            std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end);
        std::vector<LogLine> fetchLogs(const std::string &userId, const std::string &serverName, const std::string &channel,
            std::chrono::system_clock::time_point end, size_t count, bool includeEnd);
        bool insertLog(const std::string &userId, const std::string &serverName, const std::string &channel,
            const LogLine &logLine);

    private:
        std::vector<LogLine> fetchLogs(SQLite::Statement &query);

    private:
        SQLite::Database db_;

        friend class Singleton<ArchiveDB>;
    };

    class UserDB : public Singleton<UserDB>
    {
    private:
        static const std::string DBName;

    private:
        UserDB();

    public:
        std::vector<UserEntry> fetchUserEntries();
        std::string fetchPasswordHash(const std::string &userId);

    private:
        SQLite::Database db_;

        friend class Singleton<UserDB>;
    };
}
