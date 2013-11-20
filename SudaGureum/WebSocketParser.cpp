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

    WebSocketMessage::WebSocketMessage(const std::string &command, std::vector<uint8_t> &&rawData)
        : command_(command)
        , rawData_(std::move(rawData))
    {
    }

    bool WebSocketParser::IsControlFrameOpcode(WebSocketFrameOpcode opcode)
    {
        return (static_cast<uint8_t>(opcode) & 0x08) != 0;
    }

    WebSocketParser::WebSocketParser()
        : state_(WaitHandshakeHttpStatus)
        , connectionUpgrade_(false)
        , upgraded_(false)
        , versionValid_(false)
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
                    state_ = InWebSocketFrameHeader;
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
                        cb(WebSocketMessage("BadRequest"));
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
                    cb(WebSocketMessage("BadRequest"));
                    state_ = Error;
                    return false;
                }
                break;

            case WaitHandshakeEndLf:
                if(ch == '\n')
                {
                    if(host_.empty() || origin_.empty() || key_.empty() ||
                        !connectionUpgrade_ || !upgraded_ || !versionValid_)
                    {
                        cb(WebSocketMessage("BadRequest"));
                        state_ = Error;
                        return false;
                    }

                    // TODO: Message standard is not set; may be changed.
                    cb(WebSocketMessage("HandshakeRequest",
                        {{"Path", path_}, {"Host", host_}, {"Origin", origin_}, {"Key", key_}}));
                    state_ = InWebSocketFrameHeader;
                }
                else
                {
                    state_ = Error;
                    return false;
                }
                break;

            case InWebSocketFrameHeader:
                buffer_.push_back(ch);
                if(buffer_.size() == 2)
                {
                    finalFragment_ = (buffer_[0] & 0x80) != 0;
                    opcode_ = static_cast<WebSocketFrameOpcode>(buffer_[0] & 0x0F);
                    masked_ = (buffer_[1] & 0x80) != 0;
                    payloadLen1_ = (buffer_[1] & 0x7F);

                    if(payloadLen1_ < 126)
                    {
                        payloadLen_ = payloadLen1_;
                        if(!masked_)
                        {
                            if(payloadLen_ == 0)
                            {
                                parseEmptyFrame(cb);
                                buffer_.clear();
                            }
                            else
                            {
                                state_ = InPayload;
                                buffer_.clear();
                            }
                        }
                    }
                }
                else if(buffer_.size() > 2)
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
                    else if(!masked_)
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
                        else
                        {
                            if(masked_)
                            {
                                std::copy(buffer_.begin() + 2, buffer_.end(), maskingKey_.begin());
                            }
                        }

                        if(payloadLen_ == 0)
                        {
                            parseEmptyFrame(cb);
                            state_ = InWebSocketFrameHeader;
                            buffer_.clear();
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
                buffer_.push_back(ch);
                if(buffer_.size() == payloadLen_)
                {
                    parseFrame(std::move(buffer_), cb);
                    state_ = InWebSocketFrameHeader;
                    buffer_.clear();
                }
                break;
            }
        }

        return true;
    }

    bool WebSocketParser::confirmHttpStatus(std::string &&line)
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

    bool WebSocketParser::parseHttpHeader(std::string &&line)
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
        else if(boost::iequals(key, "Sec-WebSocket-Version"))
        {
            std::vector<std::string> versions;
            boost::algorithm::split(versions, value, boost::algorithm::is_any_of(","));
            versionValid_ = std::find_if(versions.begin(), versions.end(),
                [](std::string &version)
                {
                    boost::algorithm::trim(version);
                    return version == "13";
                }) != versions.end();
        }
        else if(boost::iequals(key, "Connection"))
            connectionUpgrade_ = boost::iequals(value, "Upgrade");
        else if(boost::iequals(key, "Upgrade"))
            upgraded_ = boost::iequals(value, "websocket");

        return true;
    }

    bool WebSocketParser::parseEmptyFrame(const std::function<void(const WebSocketMessage &)> &cb)
    {
        if(IsControlFrameOpcode(opcode_)) // control opcode
        {
            if(!finalFragment_) // cannot be fragmented
            {
                return false;
            }

            switch(opcode_)
            {
            case Close: // close
                cb(WebSocketMessage("Close"));
                break;

            case Ping: // ping
                return false; // cannot have payload

            case Pong: // pong
                return false; // cannot have payload
            }

            return true;
        }

        if(finalFragment_)
        {
            // TODO: play with totalPayload_

            totalPayload_.clear();
        }

        return true;
    }

    bool WebSocketParser::parseFrame(std::vector<uint8_t> &&data, const std::function<void(const WebSocketMessage &)> &cb)
    {
        if(masked_)
        {
            size_t i = 0;
            for(uint8_t &ch : data)
            {
                ch ^= maskingKey_[(i ++) % 4];
            }
        }

        if(IsControlFrameOpcode(opcode_)) // control opcode
        {
            if(!finalFragment_) // cannot be fragmented
            {
                return false;
            }

            switch(opcode_)
            {
            case Close: // close
                cb(WebSocketMessage("Close", std::move(data)));
                break;

            case Ping: // ping
                if(!masked_)
                {
                    return false; // ping payload must be masked
                }
                cb(WebSocketMessage("Ping", std::move(data)));
                break;

            case Pong: // pong
                break;
            }

            return true;
        }

        // do not consider non-control opcode

        totalPayload_.insert(totalPayload_.end(), data.begin(), data.end());

        if(finalFragment_)
        {
            // TODO: play with totalPayload_

            totalPayload_.clear();
        }

        return true;
    }

    WebSocketParser::operator bool() const
    {
        return (state_ != Error);
    }
}
