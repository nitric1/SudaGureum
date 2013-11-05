#include "Common.h"

#include "CommandLineParser.h"

namespace Batang
{
    CommandLineParser::CommandLineParser()
    {
    }

    CommandLineParser::CommandLineParser(const CommandLineParser &obj)
        : abbrs(obj.abbrs), args(obj.args)
    {
    }

    CommandLineParser::CommandLineParser(CommandLineParser &&obj)
        : abbrs(std::move(obj.abbrs)), args(std::move(obj.args))
    {
    }

    CommandLineParser::~CommandLineParser()
    {
    }

    bool CommandLineParser::addKey(const std::wstring &key, bool hasValue)
    {
        return keys.insert(std::make_pair(key, hasValue)).second;
    }

    void CommandLineParser::removeKey(const std::wstring &key)
    {
        keys.erase(key);
    }

    bool CommandLineParser::addAbbr(const std::wstring &abbr, const std::wstring &full)
    {
        return abbrs.insert(std::make_pair(abbr, full)).second;
    }

    void CommandLineParser::removeAbbr(const std::wstring &abbr)
    {
        abbrs.erase(abbr);
    }

    const std::multimap<std::wstring, std::wstring> &CommandLineParser::getArgs() const
    {
        return args;
    }

    std::vector<std::wstring> CommandLineParser::getArgs(const std::wstring &key) const
    {
        auto range = args.equal_range(key);
        std::vector<std::wstring> values;
        std::for_each(range.first, range.second, [&values](const decltype(*range.first) &value) -> void { values.push_back(value.first); });
        return values;
    }

    bool CommandLineParser::isArgEmpty(const std::wstring &key) const
    {
        return args.find(key) == args.end();
    }

    void CommandLineParser::parse(const std::wstring &commandLine)
    {
        std::vector<std::wstring> words = scan(commandLine);

        enum
        {
            NONE,
            AFTER_KEY,
        } wordToken = NONE;

        auto it = words.begin(), ed = words.end();
        std::wstring key;
        for(; it != ed; ++ it)
        {
            switch(wordToken)
            {
            case NONE:
                if(it->front() == L'-')
                {
                    if(it->size() > 1 && (*it)[1] == L'-')
                        key = it->substr(2);
                    else
                    {
                        key = it->substr(1);
                        auto fullIt = abbrs.find(key);
                        if(fullIt != abbrs.end())
                            key = fullIt->second;
                    }

                    auto keyIt = keys.find(key);
                    if(keyIt != keys.end())
                    {
                        if(keyIt->second)
                            wordToken = AFTER_KEY;
                        else
                            args.insert(std::make_pair(std::move(key), std::wstring(L"")));
                    }
                }
                else
                    args.insert(std::make_pair(std::wstring(L""), std::move(*it)));
                break;

            case AFTER_KEY:
                args.insert(std::make_pair(std::move(key), std::move(*it)));
                wordToken = NONE;
                break;
            }
        }
    }

    std::vector<std::wstring> CommandLineParser::scan(const std::wstring &commandLine)
    {
        std::vector<std::wstring> words;

        enum
        {
            NEW_WORD,
            IN_WORD,
            IN_QUOTATION,
            AFTER_QUOTATION,
        } charToken = NEW_WORD;
        std::wstring str;

        auto it = commandLine.begin(), ed = commandLine.end();
        for(; it != ed; ++ it)
        {
            switch(charToken)
            {
            case NEW_WORD:
                if(*it == L'"')
                    charToken = IN_QUOTATION;
                else if(*it != ' ')
                {
                    str += *it;
                    charToken = IN_WORD;
                }
                break;

            case IN_WORD:
                if(*it == L'"')
                    charToken = IN_QUOTATION;
                else if(*it != ' ')
                    str += *it;
                else
                {
                    words.push_back(str);
                    str.clear();
                    charToken = NEW_WORD;
                }
                break;

            case IN_QUOTATION:
                if(*it == L'"')
                    charToken = AFTER_QUOTATION;
                else
                    str += *it;
                break;

            case AFTER_QUOTATION:
                if(*it == L' ')
                {
                    words.push_back(str);
                    str.clear();
                    charToken = NEW_WORD;
                }
                else if(*it == L'"')
                {
                    str += L'"';
                    charToken = IN_QUOTATION;
                }
                else
                {
                    str += *it;
                    charToken = IN_WORD;
                }
                break;
            }
        }

        if(charToken != NEW_WORD)
        {
            words.push_back(str);
        }

        return words;
    }
}
