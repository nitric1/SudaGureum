#pragma once

namespace SudaGureum
{
    std::string scryptPassword(const std::string &password);
    bool scryptCheck(const std::string &password, const std::string &hash);
}
