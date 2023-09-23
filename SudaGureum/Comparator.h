#pragma once

namespace SudaGureum
{
    struct LessCaseInsensitive
    {
        bool operator()(const std::string &lhs, const std::string &rhs) const
        {
            return boost::algorithm::ilexicographical_compare(lhs, rhs);
        }
    };

    struct EqualToCaseInsensitive
    {
        bool operator()(const std::string &lhs, const std::string &rhs) const
        {
            return boost::algorithm::iequals(lhs, rhs);
        }
    };

    struct HashCaseInsensitive
    {
        size_t operator()(const std::string &str) const
        {
            size_t seed = 0;
            std::locale locale;

            for(char ch: str)
            {
                boost::hash_combine(seed, std::toupper(ch, locale));
            }

            return seed;
        }
    };
}
