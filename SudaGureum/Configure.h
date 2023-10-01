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

    private:
        bool load(const std::filesystem::path &file);

    public:
        bool exists(const std::string &name) const;
        std::optional<std::string> get(const std::string &name) const;
        template<typename T>
        T getAs(const std::string &name, const T &defaultValue = T()) const
        {
            auto value = get(name);
            if(!value)
                return defaultValue;
            return boost::lexical_cast<T>(value.value());
        }

    private:
        Configure &operator =(const Configure &) = delete;

    private:
        ConfigureMap confMap_;

        friend class Singleton<Configure>;
        friend class Application;
    };
}
