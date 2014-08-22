#pragma once

namespace SudaGureum
{
    std::string encodeUtf8(const std::wstring &str);
    std::wstring decodeUtf8(const std::string &str);

    std::string encodeBase64(const std::vector<uint8_t> &data);
    std::string encodeBase64(boost::any_range<uint8_t, boost::single_pass_traversal_tag> data);
    std::vector<uint8_t> decodeBase64(std::string encoded);

    std::array<uint8_t, 20> hashSha1(const std::vector<uint8_t> &data);
    std::string hashSha1ToHexString(const std::vector<uint8_t> &data);

    std::vector<uint8_t> readFile(const boost::filesystem::path &path);
}
