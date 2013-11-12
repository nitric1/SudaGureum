#pragma once

#include "Comparator.h"

namespace SudaGureum
{
    struct WebSocketMessage
    {
        typedef std::unordered_map<std::string, std::string, HashCaseInsensitive, EqualToCaseInsensitive> ParamsMap;

        std::string command_;
        ParamsMap params_;

        WebSocketMessage();
        WebSocketMessage(const std::string &command);
        WebSocketMessage(const std::string &command, const ParamsMap &params);
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
            WaitWebSocketFrameHeader,
            InWebSocketFrameHeader,
            InPayload,
            Error
        };

    public:
        WebSocketParser();

    public:
        bool parse(const std::vector<uint8_t> &data, const std::function<void (const WebSocketMessage &)> &cb);
        const std::string &host();
        const std::string &origin();
        const std::string &key();

    public:
        operator bool() const;

    private:
        bool confirmHttpStatus(const std::string &line);
        bool parseHttpHeader(const std::string &line);

    private:
        State state_;
        std::vector<uint8_t> buffer_;
        std::string path_, host_, origin_, key_;
        bool upgraded_;

        bool finalFragment_;
        uint8_t opcode_;
        bool masked_;
        uint8_t payloadLen1_;

        uint64_t payloadLen_;
        std::array<uint8_t, 4> maskingKey_;
    };
}
