#pragma once

namespace SudaGureum
{
    namespace Scrypt
    {
        std::string crypt(const std::string &password);
        bool check(const std::string &password, const std::string &hash);
    }
}
