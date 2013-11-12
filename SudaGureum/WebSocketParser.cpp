#include "Common.h"

#include "EREndian.h"

#include "WebSocketParser.h"

namespace SudaGureum
{
    WebSocketMessage::WebSocketMessage()
    {
    }

    WebSocketMessage::WebSocketMessage(const std::string &command)
        : command_(command)
    {
    }

    WebSocketMessage::WebSocketMessage(const std::string &command, const ParamsMap &params)
        : command_(command)
        , params_(params)
    {
    }

    WebSocketParser::WebSocketParser()
        : state_(WaitHandshakeHttpStatus)
        , upgraded_(false)
        , finalFragment_(false)
        , masked_(false)
        , payloadLen1_(0)
        , payloadLen_(0)
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
            case WaitHandshakeHttpStatus:
                if(ch == '\r' || ch == '\n')
                {
                    state_ = Error;
                    return false;
                }
                else
                {
                    buffer_.push_back(ch);
                    state_ = InHandshakeHttpStatus;
                }
                break;

            case InHandshakeHttpStatus:
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
                    if(!confirmHttpStatus(std::string(buffer_.begin(), buffer_.end())))
                    {
                        state_ = Error;
                        return false;
                    }
                    buffer_.clear();
                    state_ = WaitLf;
                }
                else
                {
                    buffer_.push_back(ch);
                }
                break;

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
                    if(!parseHttpHeader(std::string(buffer_.begin(), buffer_.end())))
                    {
                        state_ = Error;
                        return false;
                    }
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
                    if(host_.empty() || origin_.empty() || key_.empty() || !upgraded_)
                    {
                        state_ = Error;
                        return false;
                    }

                    cb(WebSocketMessage("Connected"));
                    state_ = WaitWebSocketFrameHeader;
                }
                else
                {
                    state_ = Error;
                    return false;
                }
                break;

            case WaitWebSocketFrameHeader:
                buffer_.push_back(ch);
                if(buffer_.size() == 2)
                {
                    finalFragment_ = (buffer_[0] & 0x80) != 0;
                    opcode_ = (buffer_[0] & 0x0F);
                    masked_ = (buffer_[1] & 0x80) != 0;
                    payloadLen1_ = (buffer_[1] & 0x7F);

                    if(!masked_ && payloadLen1_ < 126)
                    {
                        payloadLen_ = payloadLen1_;
                        if(payloadLen_ == 0)
                        {
                            // TODO: frame process
                        }
                        else
                        {
                            state_ = InPayload;
                            buffer_.clear();
                        }
                    }
                }
                else
                {
                    size_t waitFor = 2;
                    if(payloadLen1_ == 126)
                    {
                        waitFor += 2;
                    }
                    else if(payloadLen1_ == 127)
                    {
                        waitFor += 8;
                    }
                    else
                    {
                        state_ = Error; // must not be occured; assert?
                        return false;
                    }

                    if(masked_)
                    {
                        waitFor += 4;
                    }

                    if(buffer_.size() == waitFor)
                    {
                        if(payloadLen1_ == 126)
                        {
                            payloadLen_ = EREndian::B2N(*reinterpret_cast<uint16_t *>(&buffer_[2]));
                            if(masked_)
                            {
                                std::copy(buffer_.begin() + 4, buffer_.end(), maskingKey_.begin());
                            }
                        }
                        else if(payloadLen1_ == 127)
                        {
                            payloadLen_ = EREndian::B2N(*reinterpret_cast<uint64_t *>(&buffer_[2]));
                            if(masked_)
                            {
                                std::copy(buffer_.begin() + 10, buffer_.end(), maskingKey_.begin());
                            }
                        }

                        if(payloadLen_ == 0)
                        {
                            // TODO: frame process
                        }
                        else
                        {
                            state_ = InPayload;
                            buffer_.clear();
                        }
                    }
                }
                break;

            case InPayload:
                break;
            }
        }

        return true;
    }

    bool WebSocketParser::confirmHttpStatus(const std::string &line)
    {
        static const boost::regex HttpStatusRegex("GET ([^ ]+) HTTP\\/1\\.1", boost::regex::extended);
        boost::smatch match;
        if(!boost::regex_match(line, match, HttpStatusRegex))
        {
            return false;
        }

        path_ = match[1].str();
        return true;
    }

    bool WebSocketParser::parseHttpHeader(const std::string &line)
    {
        size_t colonPos = line.find(":");
        if(colonPos == std::string::npos)
        {
            return false;
        }

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        boost::algorithm::trim(key);
        if(key.empty())
        {
            return false;
        }

        boost::algorithm::trim(value);

        if(boost::iequals(key, "Host"))
            host_ = std::move(value);
        else if(boost::iequals(key, "Origin"))
            origin_ = std::move(value);
        else if(boost::iequals(key, "Sec-WebSocket-Key"))
            key_ = std::move(value);
        else if(boost::iequals(key, "Upgrade"))
            upgraded_ = boost::iequals(value, "websocket");

        return true;
    }

    WebSocketParser::operator bool() const
    {
        return (state_ != Error);
    }
}
