#include "Common.h"

#include "Batang/Utility.h"

#include "IrcParser.h"

namespace SudaGureum
{
    IrcMessage::IrcMessage()
    {
    }

    IrcMessage::IrcMessage(const std::string &command)
        : command_(command)
    {
    }

    IrcMessage::IrcMessage(const std::string &command, const std::vector<std::string> &params)
        : command_(command)
        , params_(params)
    {
    }

    IrcMessage::IrcMessage(const std::string &prefix, const std::string &command, const std::vector<std::string> &params)
        : prefix_(prefix)
        , command_(command)
        , params_(params)
    {
    }

    IrcParser::IrcParser()
        : state_(None)
    {
    }

    bool IrcParser::parse(const std::string &str, const std::function<void (const IrcMessage &)> &cb)
    {
        // TODO: cut if buffer is too large

        if(state_ == Error)
        {
            return false;
        }

        std::string line;
        for(char ch: str)
        {
            switch(state_)
            {
            case None:
                if(ch == '\r' || ch == '\n')
                {
                    state_ = Error;
                    return false;
                }
                else
                {
                    buffer_ += ch;
                    state_ = InLine;
                }
                break;

            case InLine:
                if(ch == '\r' || ch == '\n')
                {
                    if(ch == '\r')
                    {
                        state_ = WaitLf;
                    }
                    if(!parseMessage(buffer_, cb))
                    {
                        state_ = Error;
                        return false;
                    }
                    buffer_.clear();
                }
                else
                {
                    buffer_ += ch;
                }
                break;

            case WaitLf:
                if(ch != '\n')
                {
                    state_ = Error;
                    return false;
                }
                else
                {
                    state_ = None;
                }
                break;
            }
        }

        return true;
    }

    bool IrcParser::parseMessage(const std::string &line, const std::function<void (const IrcMessage &)> &cb)
    {
        enum
        {
            None,
            WaitCommand,
            InParam,
            InTrailing
        } state = None;

        std::vector<std::string> words;
        boost::algorithm::split(words, line, [](char ch) { return ch == ' '; });

        IrcMessage message;
        for(const std::string &word: words)
        {
            if(state != InTrailing && word.empty())
            {
                return false;
            }

            switch(state)
            {
            case None:
                if(word[0] == ':')
                {
                    message.prefix_ = word.substr(1);
                    if(message.prefix_.empty())
                    {
                        return false;
                    }
                    state = WaitCommand;
                }
                else
                {
                    message.command_ = word;
                    state = InParam;
                }
                break;

            case WaitCommand:
                message.command_ = word;
                state = InParam;
                break;

            case InParam:
                if(word[0] == ':')
                {
                    message.params_.push_back(word.substr(1));
                    if(message.params_.back().empty())
                    {
                        return false;
                    }
                    state = InTrailing;
                }
                else
                {
                    message.params_.push_back(word);
                    if(message.params_.size() >= 15)
                    {
                        state = InTrailing;
                    }
                }
                break;

            case InTrailing:
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
        return (state_ != Error);
    }
}
