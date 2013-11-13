#include "Common.h"

#include "Utility.h"

namespace SudaGureum
{
    std::string encodeUtf8(const std::wstring &str)
    {
#if 0
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.to_bytes(str);
#endif
        return boost::locale::conv::utf_to_utf<char>(str);
    }

    std::wstring decodeUtf8(const std::string &str)
    {
#if 0
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.from_bytes(str);
#endif
        return boost::locale::conv::utf_to_utf<wchar_t>(str);
    }
}
