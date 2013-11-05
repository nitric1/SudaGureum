#pragma once

namespace Batang
{
    class CommandLineParser
    {
    private:
        std::map<std::wstring, std::wstring> abbrs;
        std::map<std::wstring, bool> keys;
        std::multimap<std::wstring, std::wstring> args;

    public:
        CommandLineParser();
        CommandLineParser(const CommandLineParser &);
        CommandLineParser(CommandLineParser &&);
        virtual ~CommandLineParser();

    public:
        bool addKey(const std::wstring &, bool);
        void removeKey(const std::wstring &);
        bool addAbbr(const std::wstring &, const std::wstring &);
        void removeAbbr(const std::wstring &);
        const std::multimap<std::wstring, std::wstring> &getArgs() const;
        std::vector<std::wstring> getArgs(const std::wstring &) const;
        bool isArgEmpty(const std::wstring &) const;
        void parse(const std::wstring &);

    private:
        std::vector<std::wstring> scan(const std::wstring &);
    };
}
