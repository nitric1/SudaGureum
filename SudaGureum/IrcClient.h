#pragma once

#include "Comparator.h"
#include "Event.h"
#include "IrcParser.h"
#include "MtIoService.h"

namespace SudaGureum
{
    class IrcClientPool;
    class SocketBase;

    class IrcClient : private boost::noncopyable, public std::enable_shared_from_this<IrcClient>
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

            explicit Participant(std::string nickname)
                : nickname_(std::move(nickname))
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

    public:
        struct ServerMessageArgs
        {
            std::weak_ptr<IrcClient> ircClient_;
            std::string command_;
            std::string text_;
        };

        struct JoinChannelArgs
        {
            std::weak_ptr<IrcClient> ircClient_;
            std::string channel_;
            std::string nickname_; // empty if self
        };

        struct PartChannelArgs
        {
            std::weak_ptr<IrcClient> ircClient_;
            std::string channel_;
            std::string nickname_; // empty if self
        };

        struct MessageArgs
        {
            std::weak_ptr<IrcClient> ircClient_;
            std::string channel_;
            std::string nickname_;
            std::string message_;
        };

    private:
        static std::string getNicknameFromPrefix(std::string prefix);

    private:
        static const NicknamePrefixMap DefaultNicknamePrefixMap;

    private:
        IrcClient(IrcClientPool &pool, size_t connectionId);

    public:
        size_t connectionId() const;

        // self
        void nickname(const std::string &nickname);
        void mode(const std::string &modifier);

        // channel-specific; please include #'s or &'s
        void join(const std::string &channel);
        void join(const std::string &channel, const std::string &key);
        void part(const std::string &channel, const std::string &message);
        void mode(const std::string &channel, const std::string &nickname, const std::string &modifier);

        // not channel-specific
        void privmsg(const std::string &target, const std::string &message);

    public:
        Event<std::weak_ptr<IrcClient>> onConnect;
        Event<const ServerMessageArgs &> onServerMessage;
        Event<const JoinChannelArgs &> onJoinChannel;
        Event<const PartChannelArgs &> onPartChannel;
        Event<const MessageArgs &> onChannelMessage;
        Event<const MessageArgs &> onPersonalMessage;

    private:
        void connect(const std::string &host, uint16_t port, std::string encoding,
            std::vector<std::string> nicknames, bool ssl);
        void tryNextNickname();
        void read();
        void sendMessage(const IrcMessage &message);
        void write();
        void close(bool clearMe = true);
        void forceClose();
        bool isMyPrefix(const std::string &prefix) const;
        bool isChannel(const std::string &str) const;
        Participant parseParticipant(const std::string &nicknameWithPrefix) const;

    private:
        void handleRead(const boost::system::error_code &ec, size_t bytesTransferred);
        void handleWrite(const boost::system::error_code &ec, size_t bytesTransferred, const std::shared_ptr<std::string> &messagePtr);
        void handleCloseTimeout(const boost::system::error_code &ec);
        void procMessage(const IrcMessage &message);

    private:
        IrcClientPool &pool_;
        size_t connectionId_;
        boost::asio::io_service &ios_;
        std::shared_ptr<SocketBase> socket_;

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
        boost::asio::deadline_timer closeTimer_;
        bool clearMe_;

        friend class IrcClientPool;
    };

    class IrcClientPool : private boost::noncopyable, public MtIoService
    {
    public:
        IrcClientPool();

    public:
        std::weak_ptr<IrcClient> connect(const std::string &host, uint16_t port, std::string encoding,
            std::vector<std::string> nicknames, bool ssl, std::function<void (IrcClient &)> constructCb);
        void closeAll();

    private:
        void closed(const std::shared_ptr<IrcClient> &client);

    private:
        boost::asio::signal_set signals_; // TODO: temporary
        std::mutex clientsLock_;
        std::unordered_map<size_t, std::shared_ptr<IrcClient>> clients_;
        std::atomic<size_t> nextConnectionId_;

        friend class IrcClient;
    };
}
