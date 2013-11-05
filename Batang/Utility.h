#pragma once

namespace Batang
{
    std::wstring &trim(std::wstring &);
    std::vector<std::wstring> split(const std::wstring &, const std::wstring &);
    std::vector<std::wstring> splitAnyOf(const std::wstring &, const std::wstring &);
    std::wstring join(const std::vector<std::wstring> &, const std::wstring &);
    std::wstring getDirectoryPath(const std::wstring &);
    std::string encodeUTF8(const std::wstring &);
    std::wstring decodeUTF8(const std::string &);
    std::string base64Encode(const std::vector<uint8_t> &);
    std::vector<uint8_t> base64Decode(const std::string &);
    std::string encodeURI(const std::string &);
    std::string encodeURIParam(const std::string &);
    std::string decodeURI(const std::string &);

    float round(float);
    double round(double);
}
