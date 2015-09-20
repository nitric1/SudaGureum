#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    class Configure : public Singleton<Configure>
    {
    public:
        typedef std::map<std::string, std::string> ConfigureMap;

    private:
        Configure();
        Configure(const Configure &) = delete;

    private:
        ConfigureMap::const_iterator find(const std::string &name) const;

    public:
        bool load(const boost::filesystem::path &file);
        bool exists(const std::string &name) const;
        std::string get(const std::string &name, const std::string &defaultValue = std::string()) const;

    private:
        Configure &operator =(const Configure &) = delete;

    private:
        ConfigureMap confMap_;

        friend class Singleton<Configure>;
    };
}
