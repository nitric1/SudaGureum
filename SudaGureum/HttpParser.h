#pragma once

#include "Utility.h"

namespace SudaGureum
{
    struct HttpRequest
    {
        typedef CaseInsensitiveUnorderedMultimap Queries;

        enum class Method : int32_t
        {
            GET,
            HEAD,
            POST,
            OTHER, // there's no need to process other methods
        };

        Method method_;
        bool http11_; // true = HTTP 1.1, false = HTTP 1.0
        bool upgrade_;
        bool keepAlive_;
        std::string rawTarget_;
        std::string target_;
        Queries queries_;
        CaseInsensitiveUnorderedMap headers_;
        std::string rawBody_;

    private:
        HttpRequest()
            : method_(Method::OTHER)
            , http11_(false)
            , upgrade_(false)
            , keepAlive_(false)
        {
        }

        friend class HttpParser;
    };

    struct HttpResponse
    {
        uint16_t status_;
        CaseInsensitiveUnorderedMap headers_;
        std::vector<uint8_t> body_;

        HttpResponse()
            : status_(0)
        {
        }
    };

    class HttpParser
    {
    public:
        HttpParser();

    public:
        std::pair<bool /* succeeded */, size_t /* upgradedPos */> parse(
            const std::vector<uint8_t> &data, std::function<bool (const HttpRequest &)> cb);

    private:
        void appendTarget(const char *str, size_t length);
        void appendHeaderName(const char *str, size_t length);
        void appendHeaderValue(const char *str, size_t length);
        void completeHeader();
        void completeHeaders();
        void appendBody(const char *str, size_t length);
        bool completeRequest();

    private:
        http_parser parser_;
        std::function<bool (const HttpRequest &)> currentCallback_;
        HttpRequest request_;
        bool calledHeaderValue_;
        std::string currentHeaderName_;
        std::string currentHeaderValue_;

        friend static int HttpParserOnUrl(http_parser *, const char *, size_t);
        friend static int HttpParserOnHeaderField(http_parser *, const char *, size_t);
        friend static int HttpParserOnHeaderValue(http_parser *, const char *, size_t);
        friend static int HttpParserOnHeadersComplete(http_parser *);
        friend static int HttpParserOnBody(http_parser *, const char *, size_t);
        friend static int HttpParserOnMessageComplete(http_parser *);
    };
}
