#include "Common.h"

#include "HttpParser.h"

#include "Utility.h"

namespace SudaGureum
{
    static int HttpParserOnUrl(http_parser *parser, const char *str, size_t length)
    {
        static_cast<HttpParser *>(parser->data)->appendPath(str, length);
        return 0;
    }

    static int HttpParserOnHeaderField(http_parser *parser, const char *str, size_t length)
    {
        static_cast<HttpParser *>(parser->data)->appendHeaderName(str, length);
        return 0;
    }

    static int HttpParserOnHeaderValue(http_parser *parser, const char *str, size_t length)
    {
        static_cast<HttpParser *>(parser->data)->appendHeaderValue(str, length);
        return 0;
    }

    static int HttpParserOnHeadersComplete(http_parser *parser)
    {
        static_cast<HttpParser *>(parser->data)->completeHeaders();
        return 0;
    }

    static int HttpParserOnBody(http_parser *parser, const char *str, size_t length)
    {
        static_cast<HttpParser *>(parser->data)->appendBody(str, length);
        return 0;
    }

    static int HttpParserOnMessageComplete(http_parser *parser)
    {
        return !static_cast<HttpParser *>(parser->data)->completeRequest();
    }

    namespace
    {
        const http_parser_settings ParserSettings =
        {
            nullptr, // on_message_begin
            &HttpParserOnUrl, // on_url
            nullptr, // on_status
            &HttpParserOnHeaderField, // on_header_field
            &HttpParserOnHeaderValue, // on_header_value
            &HttpParserOnHeadersComplete, // on_headers_complete
            &HttpParserOnBody, // on_body
            &HttpParserOnMessageComplete, // on_message_complete
            nullptr, // on_chunk_header
            nullptr, // on_chunk_complete
        };
    }

    HttpParser::HttpParser()
        : calledHeaderValue_(false)
    {
        http_parser_init(&parser_, HTTP_REQUEST);
        parser_.data = this;
        // functions
    }

    std::pair<bool /* succeeded */, size_t /* upgradedPos */> HttpParser::parse(
        const std::vector<uint8_t> &data, std::function<bool (const HttpRequest &)> cb)
    {
        currentCallback_ = std::move(cb);

        size_t parsed = http_parser_execute(&parser_, &ParserSettings, reinterpret_cast<const char *>(data.data()), data.size());

        if(parser_.upgrade)
        {
            return {true, parsed};
        }
        else if(parsed != data.size())
        {
            return {false, 0};
        }

        return {true, 0};
    }

    void HttpParser::appendPath(const char *str, size_t length)
    {
        request_.path_.append(str, length);
    }

    void HttpParser::appendHeaderName(const char *str, size_t length)
    {
        if(calledHeaderValue_)
        {
            completeHeader();
        }

        currentHeaderName_.append(str, length);
    }

    void HttpParser::appendHeaderValue(const char *str, size_t length)
    {
        calledHeaderValue_ = true;
        currentHeaderValue_.append(str, length);
    }

    void HttpParser::completeHeader()
    {
        request_.headers_.emplace(std::move(currentHeaderName_), std::move(currentHeaderValue_));
        currentHeaderName_.clear();
        currentHeaderValue_.clear();
        calledHeaderValue_ = false;
    }

    void HttpParser::completeHeaders()
    {
        completeHeader();
    }

    void HttpParser::appendBody(const char *str, size_t length)
    {
        request_.body_.append(str, length);
    }

    bool HttpParser::completeRequest()
    {
        Finally f([this]() { request_ = HttpRequest(); });

        switch(parser_.method)
        {
#define METHOD_CASE(x) case HTTP_##x: request_.method_ = HttpRequest::x; break
            METHOD_CASE(GET);
            METHOD_CASE(HEAD);
            METHOD_CASE(POST);
#undef METHOD_CASE
        default:
            request_.method_ = HttpRequest::OTHER;
        }

        if(parser_.http_minor != 0) // treat as HTTP 1.1
        {
            request_.http11_ = true;
        }

        request_.upgrade_ = !!parser_.upgrade;
        request_.keepAlive_ = !!http_should_keep_alive(&parser_);

        return currentCallback_(request_);
    }
}
