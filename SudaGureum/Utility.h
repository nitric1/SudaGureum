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

    template<typename ValueT>
    using CaseInsensitiveMap = std::map<std::string, ValueT, LessCaseInsensitive>;

    template<typename ValueT>
    using CaseInsensitiveMultimap = std::multimap<std::string, ValueT, LessCaseInsensitive>;

    std::string encodeUtf8(const std::wstring &str);
    std::wstring decodeUtf8(const std::string &str);

    namespace
    {
        template<typename IterT>
        using ToBase64Iterator = boost::archive::iterators::base64_from_binary<
            boost::archive::iterators::transform_width<IterT, 6, 8>>;
    }

    //std::string encodeBase64(const std::vector<uint8_t> &data);
    template<std::ranges::input_range Range> requires std::same_as<std::ranges::range_value_t<Range>, uint8_t>
    std::string encodeBase64(Range &&data)
    {
        typedef decltype(std::cbegin(data)) ConstIterT;
        typedef decltype(std::cend(data)) ConstEndIterT;

        static_assert(std::is_same<ConstIterT, ConstEndIterT>::value, "type of cbegin(Range) and cend(Range) is not same");

        std::string encoded(ToBase64Iterator<ConstIterT>(std::ranges::cbegin(data)), ToBase64Iterator<ConstIterT>(std::ranges::cend(data)));
        size_t padLen = (3 - encoded.size() % 3) % 3;
        encoded.append(padLen, '=');
        return encoded;
    }
    std::vector<uint8_t> decodeBase64(std::string encoded);

    std::string decodeURIComponent(const std::string &str);
    std::string decodeQueryString(const std::string &str);

    std::string generateHttpDateTime();
    std::string generateHttpDateTime(time_t time);
    std::string generateHttpDateTime(const std::chrono::system_clock::time_point &time);

    std::array<uint8_t, 20> hashSha1(const std::vector<uint8_t> &data);
    std::string hashSha1ToHexString(const std::vector<uint8_t> &data);

    std::deque<uint8_t> readFile(const std::filesystem::path &path);
    std::vector<uint8_t> readFileIntoVector(const std::filesystem::path &path);
}
