#include "Common.h"

#include "Archive.h"

#include "Configure.h"
#include "User.h"

namespace SQLite
{
    void assertion_failed(const char *file, long line, const char *func, const char *expr, const char *message)
    {
#ifdef _DEBUG
        std::cerr << "SQLiteCpp assertion failed: " << message << "\n";
        std::cerr << "File: " << file << ", line " << line << ", function " << func << "\n";
        std::cerr << "Expression: " << expr << "\n";
#endif

        abort();
    }
}

namespace SudaGureum
{
    std::string ArchiveDB::databaseFile()
    {
        boost::filesystem::path dataPath = Configure::instance().get("data_path", "./Data");
        boost::system::error_code ec;
        if(!boost::filesystem::create_directories(dataPath, ec) && ec)
        {
            throw(std::runtime_error(fmt::format("cannot create data directory: {}", ec.message())));
            return {};
        }

        return (dataPath / "Archive.db").string();
    }

    ArchiveDB::ArchiveDB()
        : db_(databaseFile().c_str(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
    {
        SQLite::Transaction tran(db_);

        db_.exec(
            "CREATE TABLE IF NOT EXISTS Log ("
            "    idx INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    userId TEXT NOT NULL,"
            "    serverName TEXT NOT NULL,"
            "    channel TEXT NOT NULL,"
            "    logTime INTEGER NOT NULL,"
            "    nickname TEXT,"
            "    logType INTEGER NOT NULL,"
            "    message TEXT"
            ")"
            );
        db_.exec(
            "CREATE INDEX IF NOT EXISTS LogIndex ON Log (userId, serverName, channel, logTime DESC)"
            );

        tran.commit();
    }

    std::vector<LogLine> ArchiveDB::fetch(const std::string &userId, const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end)
    {
        if(begin >= end)
            return {};

        time_t beginEpoch = std::chrono::system_clock::to_time_t(begin);
        time_t endEpoch = std::chrono::system_clock::to_time_t(end);

        boost::gregorian::date beginDate = boost::posix_time::from_time_t(beginEpoch).date();
        boost::gregorian::date endDate = boost::posix_time::from_time_t(endEpoch).date();

        SQLite::Statement query(db_,
            "SELECT idx, userId, serverName, channel, logTime, nickname, logType, message FROM Log"
            " WHERE userId = @userId AND serverName = @serverName AND channel = @channel"
            " AND logTime >= @logTimeBegin AND logTime < @logTimeEnd"
            " ORDER BY idx ASC"
            );

        query.bind("@userId", userId);
        query.bind("@serverName", serverName);
        query.bind("@channel", channel);
        query.bind("@logTimeBegin", beginEpoch);
        query.bind("@logTimeEnd", endEpoch);

        return fetchLogs(query);
    }

    std::vector<LogLine> ArchiveDB::fetch(const std::string &userId, const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point end, size_t count, bool includeEnd)
    {
        if(count == 0)
            return {};

        time_t endEpoch = std::chrono::system_clock::to_time_t(end);
        boost::gregorian::date endDate = boost::posix_time::from_time_t(endEpoch).date();

        SQLite::Statement query(db_,
            "SELECT idx, userId, serverName, channel, logTime, nickname, logType, message FROM Log"
            " WHERE userId = @userId AND serverName = @serverName AND channel = @channel"
            " AND logTime < @logTimeEnd"
            " ORDER BY idx DESC"
            " LIMIT @count"
            );

        query.bind("@userId", userId);
        query.bind("@serverName", serverName);
        query.bind("@channel", channel);
        query.bind("@logTimeEnd", endEpoch + (includeEnd ? 1 : 0));
        query.bind("@count", static_cast<int64_t>(count));

        std::vector<LogLine> logs = fetchLogs(query);
        std::reverse(logs.begin(), logs.end());

        return logs;
    }

    std::vector<LogLine> ArchiveDB::fetchLogs(SQLite::Statement &query)
    {
        std::vector<LogLine> logs;

        while(query.executeStep())
        {
            time_t logTimeEpoch = query.getColumn("logTime");

            logs.push_back(LogLine
            {
                std::chrono::system_clock::from_time_t(logTimeEpoch),
                query.getColumn("nickname"),
                static_cast<LogLine::LogType>(query.getColumn("logType").getInt()),
                query.getColumn("message"),
            });
        }

        return logs;
    }

    bool ArchiveDB::insert(const std::string &userId, const std::string &serverName, const std::string &channel,
        const LogLine &logLine)
    {
        time_t epoch = std::chrono::system_clock::to_time_t(logLine.time_);

        SQLite::Statement query(db_,
            "INSERT INTO Log (userId, serverName, channel, logTime, nickname, logType, message)"
            " VALUES (@userId, @serverName, @channel, @logTime, @nickname, @logType, @message)"
            );

        query.bind("@userId", userId);
        query.bind("@serverName", serverName);
        query.bind("@channel", channel);
        query.bind("@logTime", epoch);
        query.bind("@nickname", logLine.nickname_);
        query.bind("@logType", logLine.logType_);
        query.bind("@message", logLine.message_);

        return (query.exec() > 0);
    }

    Archive::Archive(std::string userId)
        : userId_(std::move(userId))
    {
    }

    // [begin, end)
    std::vector<LogLine> Archive::read(const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end)
    {
        return ArchiveDB::instance().fetch(userId_, serverName, channel, begin, end);
    }

    // [end - lines, end] if includeEnd, [end - lines, end) otherwise
    std::vector<LogLine> Archive::read(const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point end, size_t lines, bool includeEnd)
    {
        return ArchiveDB::instance().fetch(userId_, serverName, channel, end, lines, includeEnd);
    }

    bool Archive::insert(const std::string &serverName, const std::string &channel,
        const LogLine &logLine)
    {
        return ArchiveDB::instance().insert(userId_, serverName, channel, logLine);
    }
}
