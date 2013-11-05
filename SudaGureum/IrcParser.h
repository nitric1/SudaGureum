#pragma once

namespace SudaGureum
{
    struct IrcMessage
    {
        std::string prefix_;
        std::string command_;
        std::vector<std::string> params_;
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
        std::stringstream buffer_;
    };
}
