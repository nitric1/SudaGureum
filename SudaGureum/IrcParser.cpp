#include "Common.h"

#include "IrcParser.h"

#include "Utility.h"

namespace SudaGureum
{
    IrcMessage::IrcMessage()
    {
    }

    IrcMessage::IrcMessage(std::string command)
        : command_(command)
    {
    }

    IrcMessage::IrcMessage(std::string command, std::vector<std::string> params)
        : command_(command)
        , params_(params)
    {
    }

    IrcMessage::IrcMessage(std::string prefix, std::string command, std::vector<std::string> params)
        : prefix_(prefix)
        , command_(command)
        , params_(params)
    {
    }

    IrcParser::IrcParser()
        : state_(State::None)
    {
    }

    void IrcParser::clear()
    {
        state_ = State::None;
        buffer_.clear();
    }

    bool IrcParser::parse(const std::string &str, std::function<void (const IrcMessage &)> cb)
    {
        if(state_ == State::Error)
        {
            return false;
        }

        auto append = [this](char ch)
        {
            static const size_t BufferSizeThreshold = 4096;

            buffer_ += ch;
            if(buffer_.size() > BufferSizeThreshold)
            {
                return false;
            }

            return true;
        };

        std::string line;
        for(char ch: str)
        {
            switch(state_)
            {
            case State::None:
                if(ch == '\r' || ch == '\n')
                {
                    state_ = State::Error;
                    return false;
                }
                else
                {
                    if(!append(ch))
                    {
                        state_ = State::Error;
                        return false;
                    }
                    state_ = State::InLine;
                }
                break;

            case State::InLine:
                if(ch == '\r' || ch == '\n')
                {
                    if(ch == '\r')
                    {
                        state_ = State::WaitLf;
                    }
                    if(!parseMessage(std::move(buffer_), cb))
                    {
                        state_ = State::Error;
                        return false;
                    }
                    buffer_.clear();
                }
                else
                {
                    if(!append(ch))
                    {
                        return false;
                    }
                }
                break;

            case State::WaitLf:
                if(ch != '\n')
                {
                    state_ = State::Error;
                    return false;
                }
                else
                {
                    state_ = State::None;
                }
                break;
            }
        }

        return true;
    }

    bool IrcParser::parseMessage(const std::string &line, std::function<void (const IrcMessage &)> cb)
    {
        enum class ParseMessageState : int32_t
        {
            None,
            WaitCommand,
            InParam,
            InTrailing
        } state = ParseMessageState::None;

        std::vector<std::string> words;
        boost::algorithm::split(words, line, boost::algorithm::is_any_of(" "));

        IrcMessage message;
        for(const std::string &word: words)
        {
            if(state != ParseMessageState::InTrailing && word.empty())
            {
                return false;
            }

            switch(state)
            {
            case ParseMessageState::None:
                if(word[0] == ':')
                {
                    message.prefix_ = word.substr(1);
                    if(message.prefix_.empty())
                    {
                        return false;
                    }
                    state = ParseMessageState::WaitCommand;
                }
                else
                {
                    message.command_ = word;
                    state = ParseMessageState::InParam;
                }
                break;

            case ParseMessageState::WaitCommand:
                message.command_ = word;
                state = ParseMessageState::InParam;
                break;

            case ParseMessageState::InParam:
                if(word[0] == ':')
                {
                    message.params_.push_back(word.substr(1));
                    state = ParseMessageState::InTrailing;
                }
                else
                {
                    message.params_.push_back(word);
                    if(message.params_.size() >= 15)
                    {
                        state = ParseMessageState::InTrailing;
                    }
                }
                break;

            case ParseMessageState::InTrailing:
                message.params_.back() += " " + word;
                break;
            }
        }

        static const boost::regex CommandRegex("[A-Za-z]+|[0-9]{3}", boost::regex::extended); // TODO: non-static or something
        if(!boost::regex_match(message.command_, CommandRegex))
        {
            return false;
        }

        cb(message);

        return true;
    }

    IrcParser::operator bool() const
    {
        return (state_ != State::Error);
    }

    bool IrcParser::operator !() const
    {
        return (state_ == State::Error);
    }
}
