#pragma once

#include "IrcParser.h"

namespace SudaGureum
{
    class IrcClientPool;

    class IrcClient : public std::enable_shared_from_this<IrcClient>
    {
    private:
        IrcClient(boost::asio::io_service &ios, IrcClientPool &pool);

    private:
        void connect(const std::string &addr, uint16_t port, const std::vector<std::string> &nicknames);
        void tryNextNickname();
        void read();
        void sendMessage(const IrcMessage &message);
        void write(bool force = false);
        void close(bool clearMe = true);

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
        boost::asio::signal_set signals_; // TODO: temp
        std::vector<std::shared_ptr<std::thread>> threads_;
        std::vector<std::shared_ptr<IrcClient>> clients_;

        friend class IrcClient;
    };
}
