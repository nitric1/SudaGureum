#pragma once

namespace SudaGureum
{
    struct IrcMessage
    {
        std::string prefix_;
        std::string command_;
        std::vector<std::string> params_;

        IrcMessage();
        IrcMessage(const std::string &command);
        IrcMessage(const std::string &command, const std::vector<std::string> &params);
        IrcMessage(const std::string &prefix, const std::string &command, const std::vector<std::string> &params);
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
        bool parse(const std::string &str, const std::function<void (const IrcMessage &)> &cb);

    public:
        operator bool() const;

    private:
        bool parseMessage(const std::string &line, const std::function<void (const IrcMessage &)> &cb);

    private:
        State state_;
        std::string buffer_;
    };
}
