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
}
