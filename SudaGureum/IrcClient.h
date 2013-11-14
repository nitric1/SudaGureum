#pragma once

#include "Comparator.h"
#include "IrcParser.h"
#include "MtIoService.h"

namespace SudaGureum
{
    class IrcClientPool;

    class IrcClient : public boost::noncopyable, public std::enable_shared_from_this<IrcClient>
    {
    public:
        struct Participant
        {
            enum Modes
            {
                Voice = 0,
                HalfOp,
                Op,
                Admin,
                Owner
            };

            std::bitset<5> modes_;
            std::string nickname_;
            bool away_;

            Participant()
                : away_(false)
            {
            }

            explicit Participant(const std::string &nickname)
                : nickname_(nickname)
                , away_(false)
            {
            }
        };

        typedef boost::multi_index_container<
            Participant,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_unique<
                    boost::multi_index::member<Participant, std::string, &Participant::nickname_>,
                    LessCaseInsensitive
                >
            >
        > ChannelParticipantSet;

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
            std::string topicSetter_;
            boost::posix_time::ptime topicSetTime_;
            ChannelParticipantSet participants_;
            std::string key_;
            size_t limit_;

            Channel()
                : accessivity_(0)
            {
            }
        };

        typedef std::unordered_map<std::string, Channel,
            HashCaseInsensitive, EqualToCaseInsensitive> ChannelMap;

        typedef std::pair<char, char> CcPair;
        typedef boost::multi_index_container<
            CcPair,
            boost::multi_index::indexed_by<
                boost::multi_index::ordered_unique<
                    boost::multi_index::member<CcPair, char, &CcPair::first>
                >,
                boost::multi_index::ordered_unique<
                    boost::multi_index::member<CcPair, char, &CcPair::second>
                >
            >
        > NicknamePrefixMap;

    private:
        static std::string getNicknameFromPrefix(const std::string &prefix);

    private:
        static const NicknamePrefixMap DefaultNicknamePrefixMap;

    private:
        IrcClient(IrcClientPool &pool);

    public:
        void nickname(const std::string &nickname);
        // please include #'s or &'s
        void join(const std::string &channel);
        void join(const std::string &channel, const std::string &key);
        void part(const std::string &channel, const std::string &message);
        void privmsg(const std::string &channel, const std::string &message);

    private:
        void connect(const std::string &addr, uint16_t port, const std::string &encoding,
            const std::vector<std::string> &nicknames);
        void tryNextNickname();
        void read();
        void sendMessage(const IrcMessage &message);
        void write();
        void close(bool clearMe = true);
        bool isMyPrefix(const std::string &prefix) const;
        bool isChannel(const std::string &str) const;
        Participant parseParticipant(const std::string &nicknameWithPrefix) const;

    private:
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::string> &messagePtr);
        void procMessage(const IrcMessage &message);

    private:
        IrcClientPool &pool_;
        boost::asio::io_service &ios_;
        boost::asio::ip::tcp::socket socket_;

        std::string encoding_;

        IrcParser parser_;
        std::array<char, 65536> bufferToRead_;

        std::mutex bufferWriteLock_;
        std::deque<std::string> bufferToWrite_;
        std::mutex writeLock_;
        std::atomic<bool> inWrite_;

        std::unordered_map<std::string, std::string, HashCaseInsensitive, EqualToCaseInsensitive> serverOptions_;
        std::array<std::string, 4> channelModes_;
        std::string channelTypes_;
        NicknamePrefixMap nicknamePrefixMap_;

        bool connectBeginning_;
        std::vector<std::string> nicknameCandidates_;
        size_t currentNicknameIndex_;

        std::string nickname_;

        ChannelMap channels_;

        bool quitReady_;
        bool clearMe_;

        friend class IrcClientPool;
    };

    class IrcClientPool : public boost::noncopyable, public MtIoService
    {
    public:
        IrcClientPool();

    public:
        std::weak_ptr<IrcClient> connect(const std::string &addr, uint16_t port, const std::string &encoding,
            const std::vector<std::string> &nicknames);
        void closeAll();

    private:
        void closed(const std::shared_ptr<IrcClient> &client);

    private:
        boost::asio::signal_set signals_; // TODO: temporary
        std::mutex clientsLock_;
        std::unordered_set<std::shared_ptr<IrcClient>> clients_;

        friend class IrcClient;
    };
}
