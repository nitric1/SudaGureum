#include "Common.h"

#include "HttpParser.h"

#include "Utility.h"

namespace SudaGureum
{
    static int HttpParserOnUrl(http_parser *parser, const char *str, size_t length)
    {
        static_cast<HttpParser *>(parser->data)->appendTarget(str, length);
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

    void HttpParser::appendTarget(const char *str, size_t length)
    {
        request_.rawTarget_.append(str, length);
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
        boost::trim(currentHeaderName_);
        boost::trim(currentHeaderValue_);
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
        request_.rawBody_.append(str, length);
    }

    namespace
    {
        bool parseQueryString(const std::string &rawQueryStr, HttpRequest::Queries &queries)
        {
            try
            {
                size_t prevPos = 0, nextPos = 0;
                do
                {
                    nextPos = rawQueryStr.find_first_of("&;", prevPos);
                    if(nextPos != prevPos)
                    {
                        size_t equalPos = rawQueryStr.find('=', prevPos);
                        if(equalPos != std::string::npos && equalPos < nextPos)
                        {
                            size_t nameLen = equalPos - prevPos;
                            size_t valueLen =
                                (nextPos == std::string::npos ? std::string::npos : (nextPos - (equalPos + 1)));
                            queries.insert(std::make_pair(
                                decodeQueryString(rawQueryStr.substr(prevPos, nameLen)),
                                decodeQueryString(rawQueryStr.substr(equalPos + 1, valueLen))));
                        }
                        else
                        {
                            size_t nameLen = (nextPos == std::string::npos ? std::string::npos : (nextPos - prevPos));
                            queries.insert(std::make_pair(
                                decodeQueryString(rawQueryStr.substr(prevPos, nameLen)),
                                std::string()));
                        }
                    }

                    prevPos = nextPos + 1;
                }
                while(nextPos != std::string::npos);
            }
            catch(const std::logic_error &)
            {
                return false;
            }

            return true;
        }

        bool parseRequestTarget(std::string rawTarget, std::string &target, HttpRequest::Queries &queries)
        {
            if(rawTarget.empty())
            {
                // must not occur on http-parser
                return false;
            }

            if(rawTarget[0] != '/') // if absolute-form (http://... for proxying), authority-form (CONNECT www...), or asterisk-form (OPTIONS * ...)
            {
                return false; // don't handle
            }

            size_t qsPos = rawTarget.find('?');
            target = rawTarget.substr(0, qsPos);
            if(qsPos != std::string::npos)
            {
                if(!parseQueryString(rawTarget.substr(qsPos + 1), queries))
                {
                    return false;
                }
            }

            return true;
        }
    }

    bool HttpParser::completeRequest()
    {
        Finally f([this]() { request_ = HttpRequest(); });

        switch(parser_.method)
        {
#define _(x) case HTTP_##x: request_.method_ = HttpRequest::x; break
            _(GET);
            _(HEAD);
            _(POST);
#undef _
        default:
            request_.method_ = HttpRequest::OTHER;
        }

        if(parser_.http_minor != 0) // treat as HTTP 1.1
        {
            request_.http11_ = true;
        }

        request_.upgrade_ = !!parser_.upgrade;
        request_.keepAlive_ = !!http_should_keep_alive(&parser_);

        if(!parseRequestTarget(request_.rawTarget_, request_.target_, request_.queries_))
        {
            return false;
        }

        try
        {
            // TODO: Content-Type parameters (charset, ...)
            if(parser_.method == HttpRequest::POST &&
                boost::iequals(request_.headers_.at("Content-Type"), "application/x-www-form-urlencoded"))
            {
                if(!parseQueryString(request_.rawBody_, request_.queries_))
                {
                    return false;
                }
            }
        }
        catch(const std::out_of_range &)
        {
        }

        return currentCallback_(request_);
    }
}
