#pragma once

namespace SudaGureum
{
    std::string encodeUtf8(const std::wstring &str);
    std::wstring decodeUtf8(const std::string &str);
}
