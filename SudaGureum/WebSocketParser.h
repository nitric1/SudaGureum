#pragma once

namespace SudaGureum
{
    struct WebSocketMessage
    {
        std::string command_;
    };

    class WebSocketParser
    {
    private:
        enum State
        {
            WaitHandshakeHeader,
            InHandshakeHeader,
            WaitLf,
            WaitHandshakeEndLf,
            WaitWebSocketFrameHeader,
            InWebSocketFrameHeader,
            WaitPayload,
            InPayload,
            Error
        };

    public:
        WebSocketParser();

    public:
        bool parse(const std::vector<uint8_t> &data, const std::function<void (const WebSocketMessage &)> &cb);

    private:
        State state_;
        std::vector<uint8_t> buffer_;
        std::map<std::string, std::string> headers_;
    };
}
