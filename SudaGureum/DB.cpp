#include "Common.h"

#include "DB.h"

#include "Configure.h"
#include "Log.h"

namespace SQLite // SQLiteCpp custom function implementation
{
    void assertion_failed(const char *file, long line, const char *func, const char *expr, const char *message)
    {
#ifdef _DEBUG
        auto &log = SudaGureum::Log::instance();
        log.critical("SQLiteCpp assertion failed: {}", message);
        log.critical("File: {}, line: {}, function: {}", file, line, func);
        log.critical("Expression: {}", expr);
#endif

        abort();
    }
}

namespace SudaGureum
{
    // TODO: db versioning

    namespace
    {
        std::string databaseFile(const std::string &dbName)
        {
            std::filesystem::path dataPath = decodeUtf8(Configure::instance().get("data_path").value_or("./Data"));
            std::error_code ec;
            if(!std::filesystem::create_directories(dataPath, ec) && ec)
            {
                throw(std::runtime_error(std::format("cannot create data directory: {}", ec.message())));
                return {};
            }

            return (dataPath / (dbName + ".db")).string();
        }
    }

    const std::string ArchiveDB::DBName = "Archive";

    ArchiveDB::ArchiveDB()
        : db_(databaseFile(DBName).c_str(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
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

    std::vector<LogLine> ArchiveDB::fetchLogs(const std::string &userId, const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point begin, std::chrono::system_clock::time_point end)
    {
        if(begin >= end)
            return {};

        time_t beginEpoch = std::chrono::system_clock::to_time_t(begin);
        time_t endEpoch = std::chrono::system_clock::to_time_t(end);

        // boost::gregorian::date beginDate = boost::posix_time::from_time_t(beginEpoch).date();
        // boost::gregorian::date endDate = boost::posix_time::from_time_t(endEpoch).date();

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

    std::vector<LogLine> ArchiveDB::fetchLogs(const std::string &userId, const std::string &serverName, const std::string &channel,
        std::chrono::system_clock::time_point end, size_t count, bool includeEnd)
    {
        if(count == 0)
            return {};

        time_t endEpoch = std::chrono::system_clock::to_time_t(end);
        // boost::gregorian::date endDate = boost::posix_time::from_time_t(endEpoch).date();

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

    bool ArchiveDB::insertLog(const std::string &userId, const std::string &serverName, const std::string &channel,
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

    const std::string UserDB::DBName = "User";

    UserDB::UserDB()
        : db_(databaseFile(DBName).c_str(), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE)
    {
        SQLite::Transaction tran(db_);

        db_.exec(
            "CREATE TABLE IF NOT EXISTS User ("
            "    idx INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    userId TEXT NOT NULL,"
            "    passwordHash TEXT NOT NULL"
            ")"
            );
        db_.exec(
            "CREATE INDEX IF NOT EXISTS UserIndex ON User (userId)"
            );

        db_.exec(
            "CREATE TABLE IF NOT EXISTS UserServer ("
            "    idx INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    userIdx INTEGER NOT NULL,"
            "    serverName TEXT NOT NULL,"
            "    host TEXT NOT NULL,"
            "    port INTEGER NOT NULL"
            ")"
            );

        tran.commit();
    }

    std::vector<UserEntry> UserDB::fetchUserEntries()
    {
        SQLite::Statement query(db_, "SELECT userId FROM User");

        while(query.executeStep())
        {
            std::string userId = query.getColumn("userId");
        }

        return {};
    }

    std::string UserDB::fetchPasswordHash(const std::string &userId)
    {
        SQLite::Statement query(db_,
            "SELECT userId, passwordHash FROM User"
            " WHERE userId = @userId"
            );

        query.bind("@userId", userId);

        if(!query.executeStep())
        {
            return {};
        }

        return query.getColumn("passwordHash");
    }
}
