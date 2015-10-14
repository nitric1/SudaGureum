#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    struct LogLine
    {
        enum LogType
        {
            JOIN,
            PART,
            MODE,
            TOPIC,
            NOTICE,
            PRIVMSG
        };

        std::chrono::system_clock::time_point time_;
        std::string nickname_;
        LogType logType_;
        std::string message_;
    };

    class Archive
    {
    private:
        Archive(std::string userId);

    public:
        std::vector<LogLine> read(const std::string &serverName, const std::string &channel,
            std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end);
        std::vector<LogLine> read(const std::string &serverName, const std::string &channel,
            std::chrono::system_clock::time_point end, size_t lines, bool includeEnd);
        bool insert(const std::string &serverName, const std::string &channel, const LogLine &logLine);

    private:
        std::string userId_;

        friend class User;
    };
}
