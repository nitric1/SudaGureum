#pragma once

namespace SudaGureum
{
    struct IrcMessage
    {
        std::string prefix_;
        std::string command_;
        std::vector<std::string> params_;

        IrcMessage();
        IrcMessage(std::string command);
        IrcMessage(std::string command, std::vector<std::string> params);
        IrcMessage(std::string prefix, std::string command, std::vector<std::string> params);
    };

    class IrcParser
    {
    private:
        enum State
        {
            None,
            InLine,
            WaitLf,
            Error
        };

    public:
        IrcParser();

    public:
        bool parse(const std::string &str, std::function<void (const IrcMessage &)> cb);

    public:
        explicit operator bool() const;

    private:
        bool parseMessage(const std::string &line, std::function<void(const IrcMessage &)> cb);

    private:
        State state_;
        std::string buffer_;
    };
}
