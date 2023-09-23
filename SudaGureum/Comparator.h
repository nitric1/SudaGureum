#pragma once

namespace SudaGureum
{
    struct LessCaseInsensitive : public std::binary_function<std::string, std::string, bool>
    {
        bool operator()(const std::string &lhs, const std::string &rhs) const
        {
            return boost::algorithm::ilexicographical_compare(lhs, rhs);
        }
    };

    struct EqualToCaseInsensitive : public std::binary_function<std::string, std::string, bool>
    {
        bool operator()(const std::string &lhs, const std::string &rhs) const
        {
            return boost::algorithm::iequals(lhs, rhs);
        }
    };

    struct HashCaseInsensitive : public std::unary_function<std::string, size_t>
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
