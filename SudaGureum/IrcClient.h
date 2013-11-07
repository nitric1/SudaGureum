#pragma once

#include "IrcParser.h"

namespace SudaGureum
{
    class IrcClientPool;

    class IrcClient : public std::enable_shared_from_this<IrcClient>
    {
    private:
        struct LessCaseInsensitive : public std::binary_function<std::string, std::string, bool>
        {
            bool operator()(const std::string &lhs, const std::string &rhs) const
            {
                return boost::algorithm::ilexicographical_compare(lhs, rhs);
            }
        };

        struct EqualToCaseInsensitive : public std::binary_function<std::string, std::string, bool>
        {
            bool operator()(const std::string &lhs, const std::string &rhs) const
            {
                return boost::algorithm::iequals(lhs, rhs);
            }
        };

        struct HashCaseInsensitive : public std::unary_function<std::string, size_t>
        {
            size_t operator()(const std::string &str) const
            {
                size_t seed = 0;
                std::locale locale;

                for(char ch: str)
                {
                    boost::hash_combine(seed, std::toupper(ch, locale));
                }

                return seed;
            }
        };

    public:
        typedef std::set<std::string, LessCaseInsensitive> ChannelParticipantSet;

        struct Channel
        {
            enum Accessivity
            {
                Public = '=',
                Private = '*',
                Secret = '@'
            };
            char accessivity_;
            std::string topic_;
            ChannelParticipantSet participants_;
        };

        typedef std::unordered_map<std::string, Channel,
            HashCaseInsensitive, EqualToCaseInsensitive> ChannelMap;

    private:
        static std::string getNicknameFromPrefix(const std::string &prefix);

    private:
        IrcClient(boost::asio::io_service &ios, IrcClientPool &pool);

    public:
        void nickname(const std::string &nickname);
        // please include #'s or &'s
        void join(const std::string &channel);
        void join(const std::string &channel, const std::string &key);
        void part(const std::string &channel, const std::string &message);
        void privmsg(const std::string &channel, const std::string &message);

    private:
        void connect(const std::string &addr, uint16_t port, const std::vector<std::string> &nicknames);
        void tryNextNickname();
        void read();
        void sendMessage(const IrcMessage &message);
        void write(bool force = false);
        void close(bool clearMe = true);
        bool isMyPrefix(const std::string &prefix);

    private:
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::string> &messagePtr);
        void procMessage(const IrcMessage &message);

    private:
        IrcClientPool &pool_;
        boost::asio::io_service &ios_;
        boost::asio::ip::tcp::socket socket_;

        IrcParser parser_;
        std::array<char, 65536> bufferToRead_;

        std::mutex bufferWriteLock_;
        std::deque<std::string> bufferToWrite_;
        std::atomic<bool> inWrite_;

        bool connectBeginning_;
        std::vector<std::string> nicknameCandidates_;
        size_t currentNicknameIndex_;
        std::string nickname_;

        ChannelMap channels_;

        bool quitReady_;
        bool clearMe_;

        friend class IrcClientPool;
    };

    class IrcClientPool
    {
    public:
        IrcClientPool();

    public:
        void run(int numThreads);
        void join();
        std::weak_ptr<IrcClient> connect(const std::string &addr, uint16_t port, const std::vector<std::string> &nicknames);
        void closeAll();

    private:
        void closed(const std::shared_ptr<IrcClient> &client);
        void stop();

    private:
        boost::asio::io_service ios_;
        boost::asio::signal_set signals_; // TODO: temporary
        std::vector<std::shared_ptr<std::thread>> threads_;
        std::mutex clientsLock_;
        std::unordered_set<std::shared_ptr<IrcClient>> clients_;

        friend class IrcClient;
    };
}
