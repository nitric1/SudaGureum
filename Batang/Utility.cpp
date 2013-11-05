#include "Common.h"

#include "Utility.h"

namespace Batang
{
    std::wstring &trim(std::wstring &str)
    {
        static auto fn = [](wchar_t ch) -> wchar_t { return ch != L' ' && ch != L'　' && ch != L'\t' && ch != L'\n' && ch != L'\r'; };
        str.erase(str.begin(), find_if(str.begin(), str.end(), fn));
        str.erase(find_if(str.rbegin(), str.rend(), fn).base(), str.end());
        return str;
    }

    std::vector<std::wstring> split(const std::wstring &str, const std::wstring &sep)
    {
        size_t ppos, pos = static_cast<size_t>(-static_cast<ptrdiff_t>(sep.size()));
        std::vector<std::wstring> ve;

        while(true)
        {
            ppos = pos + sep.size();
            pos = str.find(sep, ppos);

            if(pos == std::wstring::npos)
            {
                ve.push_back(str.substr(ppos));
                break;
            }
            else
                ve.push_back(str.substr(ppos, pos - ppos));
        }

        return ve;
    }

    std::vector<std::wstring> splitAnyOf(const std::wstring &str, const std::wstring &sep)
    {
        size_t ppos, pos = static_cast<size_t>(-1);
        std::vector<std::wstring> ve;

        while(true)
        {
            ppos = pos;
            pos = str.find_first_of(sep, ++ ppos);

            if(pos == std::wstring::npos)
            {
                ve.push_back(str.substr(ppos));
                break;
            }
            else
                ve.push_back(str.substr(ppos, pos - ppos));
        }

        return ve;
    }

    std::wstring join(const std::vector<std::wstring> &ve, const std::wstring &sep)
    {
        if(ve.empty())
            return std::wstring();

        std::wstring str = ve.front();
        for(auto it = ++ ve.begin(); it != ve.end(); ++ it)
        {
            str += sep;
            str += *it;
        }

        return str;
    }

    std::wstring getDirectoryPath(const std::wstring &ipath)
    {
        std::wstring path = ipath;
        size_t pos = path.rfind(L'\\');
        if(pos != std::wstring::npos)
            path.erase(++ pos);
        return path;
    }

    std::string encodeUTF8(const std::wstring &str)
    {
        std::vector<char> result(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr)) + 1);
        WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &*result.begin(), static_cast<int>(result.size()), nullptr, nullptr);
        return &*result.begin();
    }

    std::wstring decodeUTF8(const std::string &str)
    {
        std::vector<wchar_t> result(static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0)) + 1);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &*result.begin(), static_cast<int>(result.size()));
        return &*result.begin();
    }

    std::string base64Encode(const std::vector<uint8_t> &data)
    {
        static const char hashChar[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        static const char padding = '=';

        std::string str;
        str.reserve((data.size() + 2) * 4 / 3);
        uint8_t buf[3];
        char cand[4];
        size_t i = 0;

        for(auto it = data.begin(), ed = data.end(); it != ed; ++ it)
        {
            buf[i ++] = *it;
            if(i >= 3)
            {
                cand[0] = hashChar[(buf[0] & 0xFC) >> 2];
                cand[1] = hashChar[((buf[0] & 0x03) << 4) | ((buf[1] & 0xF0) >> 4)];
                cand[2] = hashChar[((buf[1] & 0x0F) << 2) | ((buf[2] & 0xC0) >> 6)];
                cand[3] = hashChar[buf[2] & 0x3F];
                str.insert(str.end(), cand, cand + 4);
                i = 0;
            }
        }

        if(i > 0)
        {
            size_t j;
            for(j = i; j < 3; ++ j)
                buf[j] = 0;
            cand[0] = hashChar[(buf[0] & 0xFC) >> 2];
            cand[1] = hashChar[((buf[0] & 0x03) << 4) | ((buf[1] & 0xF0) >> 4)];
            cand[2] = hashChar[((buf[1] & 0x0F) << 2) | ((buf[2] & 0xC0) >> 6)];
            cand[3] = hashChar[buf[2] & 0x3F];
            str.insert(str.end(), cand, cand + (i + 1));
            while(++ i <= 3)
                str.push_back('=');
        }

        return str;
    }

    namespace
    {
        inline bool isBase64(char ch)
        {
            return isalnum(ch) || ch == '+' || ch == '/';
        }
    }

    //std::vector<uint8_t> base64Decode(const std::string &str)
    //{
    //}

    namespace
    {
        char hexDigits[16] =
        {
            '0', '1', '2', '3', '4', '5', '6', '7',
            '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        };

        int hexDigitToNum(char ch)
        {
            if(ch >= '0' && ch <= '9')
                return ch - '0';
            else if(ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            else if(ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            return -1;
        }

        inline bool isSafeURIChar(char ch)
        {
            return (ch >= '0' && ch <= '9') // <DIGIT>
                || (ch >= 'A' && ch <= 'Z') // <UPPER>
                || (ch >= 'a' && ch <= 'z') // <LOWER>
                || (ch == '$' || ch == '-' || ch == '_' || ch == '@' || ch == '.') // <SAFE>
                || (ch == '!' || ch == '*' || ch == '"' || ch == '\''
                    || ch == '(' || ch == ')' || ch == ','); // <EXTRA>
        }
    }

    std::string encodeURI(const std::string &str)
    {
        std::string buf;
        buf.reserve(str.size());
        for(auto it = str.begin(); it != str.end(); ++ it)
        {
            if(isSafeURIChar(*it))
                buf.push_back(*it);
            else
            {
                buf += "%";
                buf.push_back(hexDigits[*it >> 4]);
                buf.push_back(hexDigits[*it & 0x0F]);
            }
        }

        return buf;
    }

    std::string encodeURIParam(const std::string &str)
    {
        std::string buf;
        buf.reserve(str.size());
        for(auto it = str.begin(); it != str.end(); ++ it)
        {
            if(isSafeURIChar(*it))
                buf.push_back(*it);
            else if(*it == ' ')
                buf += "+";
            else
            {
                buf += "%";
                buf.push_back(hexDigits[*it >> 4]);
                buf.push_back(hexDigits[*it & 0x0F]);
            }
        }
            
        return buf;
    }

    std::string decodeURI(const std::string &str)
    {
        std::string buf;
        char percentFirstChar;
        int num;
        enum
        {
            NORMAL,
            IN_PERCENT_FIRST,
            IN_PERCENT_SECOND
        } token = NORMAL;
        for(auto it = str.begin(); it != str.end(); ++ it)
        {
            switch(token)
            {
            case NORMAL:
                if(*it == '%')
                    token = IN_PERCENT_FIRST;
                else
                    buf += *it;
                break;

            case IN_PERCENT_FIRST:
                if(hexDigitToNum(*it))
                {
                    buf += '%';
                    buf += *it;
                    token = NORMAL;
                }
                else
                {
                    percentFirstChar = *it;
                    token = IN_PERCENT_SECOND;
                }
                break;

            case IN_PERCENT_SECOND:
                num = hexDigitToNum(*it);
                if(num < 0)
                {
                    buf += '%';
                    buf += percentFirstChar;
                    buf += *it;
                }
                else
                    buf += static_cast<char>(hexDigitToNum(percentFirstChar) << 4 | num);
                token = NORMAL;
                break;
            }
        }
        return buf;
    }

    float round(float n)
    {
        return floor(n + 0.5f);
    }

    double round(double n)
    {
        return floor(n + 0.5);
    }
}
