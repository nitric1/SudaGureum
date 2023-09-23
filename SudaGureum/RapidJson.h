#pragma once

namespace SudaGureum
{
    namespace RapidJson
    {
        class Exception : public std::logic_error
        {
        public:
            using std::logic_error::logic_error;
        };
    }
}

// #define RAPIDJSON_ASSERT(x) if((x)) {} else throw ::SudaGureum::RapidJson::Exception(#x)
#define RAPIDJSON_HAS_STDSTRING 1

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
