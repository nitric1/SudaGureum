#include "Common.h"

#include "Configure.h"

#include "Utility.h"

namespace SudaGureum
{
    Configure::Configure()
    {
    }

    bool Configure::load(const std::filesystem::path &file)
    {
        std::ifstream ifp(file, std::ifstream::binary);
        if(!ifp)
            return false;

        auto fileBeginPos = ifp.tellg();
        ifp.seekg(0, std::ifstream::end);
        size_t fileSize = ifp.tellg() - fileBeginPos;
        ifp.seekg(0, std::ifstream::beg);

        std::vector<char> buffer(fileSize);
        if(!ifp.read(buffer.data(), fileSize))
            return false;

        ifp.close();

        buffer.push_back('\0');
        auto it = buffer.begin();
        if(buffer.size() >= 3) // skip bom
        {
            if(*it == '\xEF' && *(it + 1) == '\xBB' && *(it + 2) == '\xBF')
                it += 3;
        }

        std::string str(&*it), name, value;
        std::istringstream is(str);
        size_t pos;

        while(!is.eof())
        {
            std::getline(is, str);

            pos = str.find('=');
            if(pos == std::string::npos)
                continue;

            name.assign(str.begin(), str.begin() + pos);
            value.assign(str.begin() + pos + 1, str.end());

            boost::algorithm::trim(name);
            boost::algorithm::trim(value);

            if(name.empty())
                continue;
            else if(name[0] == '#') // comment
                continue;

            confMap_.emplace(std::move(name), std::move(value));
        }

        return true;
    }

    Configure::ConfigureMap::const_iterator Configure::find(const std::string &name) const
    {
        return confMap_.find(name);
    }

    bool Configure::exists(const std::string &name) const
    {
        return find(name) != confMap_.end();
    }

    std::string Configure::get(const std::string &name, const std::string &defaultValue) const
    {
        auto it = find(name);
        if(it == confMap_.end())
            return defaultValue;
        return it->second;
    }
}
