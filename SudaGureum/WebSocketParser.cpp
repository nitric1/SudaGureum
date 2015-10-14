#include "Common.h"

#include "WebSocketParser.h"

#include "Log.h"

namespace SudaGureum
{
    WebSocketRequest::WebSocketRequest()
    {
    }

    WebSocketRequest::WebSocketRequest(Command command)
        : command_(command)
    {
    }

    WebSocketRequest::WebSocketRequest(Command command, ParamsMap params)
        : command_(command)
        , params_(std::move(params))
    {
    }

    WebSocketRequest::WebSocketRequest(Command command, std::vector<uint8_t> rawData)
        : command_(command)
        , rawData_(std::move(rawData))
    {
    }

    WebSocketResponse::WebSocketResponse()
    {
    }

    WebSocketResponse::WebSocketResponse(Command command)
        : command_(command)
    {
    }

    WebSocketResponse::WebSocketResponse(Command command, std::vector<uint8_t> rawData)
        : command_(command)
        , rawData_(std::move(rawData))
    {
    }

    WebSocketFrameOpcode WebSocketResponse::opcode() const
    {
        switch(command_)
        {
        case Close:
            return SudaGureum::Close;

        case Pong:
            return SudaGureum::Pong;

        case Text:
            return SudaGureum::Text;
        }

        throw(std::invalid_argument("WebSocketResponse: unknown command"));
    }

    SudaGureumResponse::SudaGureumResponse()
        : id_(0)
        , success_(false)
    {
    }

    SudaGureumResponse::SudaGureumResponse(const SudaGureumRequest &request)
        : id_(request.id_)
        , success_(false)
    {
    }

    SudaGureumResponse::SudaGureumResponse(const SudaGureumRequest &request, bool success)
        : id_(request.id_)
        , success_(success)
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

    bool WebSocketParser::parse(const std::vector<uint8_t> &data,
        std::function<void (const WebSocketRequest &)> wscb,
        std::function<void (const SudaGureumRequest &)> sgcb)
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
                        wscb(WebSocketRequest(WebSocketRequest::BadRequest));
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
                    wscb(WebSocketRequest(WebSocketRequest::BadRequest));
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
                        wscb(WebSocketRequest(WebSocketRequest::BadRequest));
                        state_ = Error;
                        return false;
                    }

                    // TODO: Message standard is not set; may be changed.
                    wscb(WebSocketRequest(WebSocketRequest::HandshakeRequest,
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
                                parseEmptyFrame(wscb, sgcb);
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
                            payloadLen_ = boost::endian::big_to_native(*reinterpret_cast<uint16_t *>(&buffer_[2]));
                            if(masked_)
                            {
                                std::copy(buffer_.begin() + 4, buffer_.end(), maskingKey_.begin());
                            }
                        }
                        else if(payloadLen1_ == 127)
                        {
                            payloadLen_ = boost::endian::big_to_native(*reinterpret_cast<uint64_t *>(&buffer_[2]));
                            if(masked_)
                            {
                                std::copy(buffer_.begin() + 10, buffer_.end(), maskingKey_.begin());
                            }
                        }
                        else
                        {
                            if(!masked_)
                            {
                                std::copy(buffer_.begin() + 2, buffer_.end(), maskingKey_.begin());
                            }
                        }

                        if(payloadLen_ == 0)
                        {
                            parseEmptyFrame(wscb, sgcb);
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
                    parseFrame(std::move(buffer_), wscb, sgcb);
                    state_ = InWebSocketFrameHeader;
                    buffer_.clear();
                }
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

    bool WebSocketParser::parseEmptyFrame(std::function<void (const WebSocketRequest &)> wscb,
        std::function<void (const SudaGureumRequest &)> sgcb)
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
                wscb(WebSocketRequest(WebSocketRequest::Close));
                break;

            case Ping: // ping
                return false; // must have payload

            case Pong: // pong
                return false; // must have payload
            }

            return true;
        }

        if(finalFragment_)
        {
            if(!parsePayload(sgcb))
            {
                return false;
            }

            totalPayload_.clear();
        }

        return true;
    }

    bool WebSocketParser::parseFrame(std::vector<uint8_t> data,
        std::function<void (const WebSocketRequest &)> wscb,
        std::function<void (const SudaGureumRequest &)> sgcb)
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
                wscb(WebSocketRequest(WebSocketRequest::Close, std::move(data)));
                break;

            case Ping: // ping
                if(!masked_)
                {
                    return false; // ping payload must be masked
                }
                wscb(WebSocketRequest(WebSocketRequest::Ping, std::move(data)));
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
            if(!parsePayload(sgcb))
            {
                return false;
            }

            totalPayload_.clear();
        }

        return true;
    }

    bool WebSocketParser::parsePayload(std::function<void (const SudaGureumRequest &)> cb)
    {
        rapidjson::Document doc;

        totalPayload_.push_back('\0');

        Log::instance().trace("WebSocketParser: parse json: {}",
            reinterpret_cast<const char *>(totalPayload_.data()));

        if(doc.ParseInsitu<0>(reinterpret_cast<char *>(totalPayload_.data())).HasParseError())
        {
            return false;
        }

        try // RPC message (client > server)
        {
            SudaGureumRequest message;

            message.id_ = doc["_reqid"].GetInt();
            message.method_ = doc["_method"].GetCppString();

            // TODO: more specific message

            cb(message);
        }
        catch(const RapidJson::Exception &e)
        {
            Log::instance().alert("WebSocketParser: parse json failed: {}", e.what());
        }
        catch(const std::out_of_range &e)
        {
            Log::instance().alert("WebSocketParser: lack of required argument: {}", e.what());
        }

        return true;
    }

    WebSocketParser::operator bool() const
    {
        return (state_ != Error);
    }
}
