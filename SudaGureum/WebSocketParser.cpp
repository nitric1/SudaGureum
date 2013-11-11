#include "Common.h"

#include "WebSocketParser.h"

namespace SudaGureum
{
    WebSocketParser::WebSocketParser()
        : state_(WaitHandshakeHeader)
    {
    }

    bool WebSocketParser::parse(const std::vector<uint8_t> &data, const std::function<void (const WebSocketMessage &)> &cb)
    {
        if(state_ == Error)
        {
            return false;
        }

        for(uint8_t ch: data)
        {
            switch(state_)
            {
            case WaitHandshakeHeader:
                if(ch == '\r')
                {
                    state_ = WaitHandshakeEndLf;
                }
                else if(ch == '\n')
                {
                    state_ = WaitWebSocketFrameHeader;
                }
                else
                {
                    buffer_.push_back(ch);
                    state_ = InHandshakeHeader;
                }
                break;

            case InHandshakeHeader:
                if(ch == '\r' || ch == '\n')
                {
                    if(ch == '\r')
                    {
                        state_ = WaitLf;
                    }
                    else if(ch == '\n')
                    {
                        state_ = WaitHandshakeHeader;
                    }

                    // TODO: parse header line
                    buffer_.clear();
                }
                else
                {
                    buffer_.push_back(ch);
                }
                break;

            case WaitLf:
                if(ch == '\n')
                {
                    state_ = WaitHandshakeHeader;
                }
                else
                {
                    state_ = Error;
                    return false;
                }
                break;

            case WaitHandshakeEndLf:
                if(ch == '\n')
                {
                    state_ = WaitWebSocketFrameHeader;
                }
                else
                {
                    state_ = Error;
                    return false;
                }
                break;

                // TODO: parse websocket frame
            }
        }

        return true;
    }
}
