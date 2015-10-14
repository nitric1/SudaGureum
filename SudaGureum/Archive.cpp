#include "Common.h"

#include "Archive.h"

#include "Configure.h"
#include "DB.h"
#include "Default.h"
#include "User.h"

namespace SudaGureum
{
    Archive::Archive(std::string userId)
        : userId_(std::move(userId))
    {
    }

    // [begin, end)
    std::vector<LogLine> Archive::read(const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end)
    {
        return ArchiveDB::instance().fetchLogs(userId_, serverName, channel, begin, end);
    }

    // [end - lines, end] if includeEnd, [end - lines, end) otherwise
    std::vector<LogLine> Archive::read(const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point end, size_t lines, bool includeEnd)
    {
        return ArchiveDB::instance().fetchLogs(userId_, serverName, channel, end, lines, includeEnd);
    }

    bool Archive::insert(const std::string &serverName, const std::string &channel,
        const LogLine &logLine)
    {
        return ArchiveDB::instance().insertLog(userId_, serverName, channel, logLine);
    }
}
