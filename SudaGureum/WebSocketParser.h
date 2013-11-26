#pragma once

#include "Comparator.h"

namespace SudaGureum
{
    enum WebSocketFrameOpcode
    {
        // Non-control frame
        Continuation = 0x0,
        Text = 0x1,
        Binary = 0x2,

        // Control frame
        Close = 0x8,
        Ping = 0x9,
        Pong = 0xA
    };

    struct WebSocketMessage
    {
        typedef std::unordered_map<std::string, std::string, HashCaseInsensitive, EqualToCaseInsensitive> ParamsMap;

        std::string command_;
        ParamsMap params_;
        std::vector<uint8_t> rawData_;

        WebSocketMessage();
        WebSocketMessage(const std::string &command);
        WebSocketMessage(const std::string &command, const ParamsMap &params);
        WebSocketMessage(const std::string &command, std::vector<uint8_t> &&rawData);
    };

    class WebSocketParser
    {
    private:
        enum State
        {
            WaitHandshakeHttpStatus,
            InHandshakeHttpStatus,
            WaitHandshakeHeader,
            InHandshakeHeader,
            WaitLf,
            WaitHandshakeEndLf,
            InWebSocketFrameHeader,
            InPayload,
            Error
        };

    private:
        static bool IsControlFrameOpcode(WebSocketFrameOpcode opcode);

    public:
        WebSocketParser();

    public:
        bool parse(const std::vector<uint8_t> &data, const std::function<void (const WebSocketMessage &)> &cb);

    public:
        explicit operator bool() const;

    private:
        bool confirmHttpStatus(std::string &&line);
        bool parseHttpHeader(std::string &&line);
        bool parseEmptyFrame(const std::function<void(const WebSocketMessage &)> &cb);
        bool parseFrame(std::vector<uint8_t> &&payload, const std::function<void(const WebSocketMessage &)> &cb);
        bool parsePayload();

    private:
        State state_;
        std::vector<uint8_t> buffer_;
        std::string path_, host_, origin_, key_;
        bool connectionUpgrade_, upgraded_, versionValid_;

        bool finalFragment_;
        WebSocketFrameOpcode opcode_;
        bool masked_;
        uint8_t payloadLen1_;

        uint64_t payloadLen_;
        std::array<uint8_t, 4> maskingKey_;

        std::vector<uint8_t> totalPayload_;
    };
}
