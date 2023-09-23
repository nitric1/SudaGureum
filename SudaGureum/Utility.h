#pragma once

#include "Comparator.h"

namespace SudaGureum
{
    class Finally final
    {
    public:
        Finally() = delete;
        Finally(std::function<void ()> fn) : fn_(std::move(fn)) {}
        Finally(const Finally &) = delete;
        Finally(Finally &&) = delete;
        Finally &operator =(const Finally &) = delete;
        Finally &operator =(Finally &&) = delete;
        ~Finally() { fn_(); }

    private:
        std::function<void ()> fn_;
    };

    typedef std::unordered_map<std::string, std::string, HashCaseInsensitive, EqualToCaseInsensitive>
        CaseInsensitiveUnorderedMap;
    typedef std::unordered_multimap<std::string, std::string, HashCaseInsensitive, EqualToCaseInsensitive>
        CaseInsensitiveUnorderedMultimap;

    std::string encodeUtf8(const std::wstring &str);
    std::wstring decodeUtf8(const std::string &str);

    std::string encodeBase64(const std::vector<uint8_t> &data);
    std::string encodeBase64(boost::any_range<uint8_t, boost::single_pass_traversal_tag> data);
    std::vector<uint8_t> decodeBase64(std::string encoded);

    std::string decodeURIComponent(const std::string &str);
    std::string decodeQueryString(const std::string &str);

    std::string generateHttpDateTime(time_t time);
    std::string generateHttpDateTime(const std::chrono::system_clock::time_point &time);

    std::array<uint8_t, 20> hashSha1(const std::vector<uint8_t> &data);
    std::string hashSha1ToHexString(const std::vector<uint8_t> &data);

    std::deque<uint8_t> readFile(const boost::filesystem::path &path);
    std::vector<uint8_t> readFileIntoVector(const boost::filesystem::path &path);
}
