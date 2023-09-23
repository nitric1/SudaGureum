#pragma once

// Definitions for Windows

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _VARIADIC_MAX 10
#define _SCL_SECURE_NO_WARNINGS

// https://docs.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers
#define WINVER 0x0A00 // Windows 10
#define _WIN32_WINDOWS 0x0A00
#define _WIN32_WINNT 0x0A00
#define _WIN32_IE 0x0A00 // Internet Explorer 11
#define NTDDI_VERSION 0x0A000007 // Windows 10, 1903

// wstring_convert and codecvt_utf8_utf16 (and some more typedef's) are deprecated since C++17,
// but there are no replacements at all; I'll use deprecated ones until some replacements are standardized.
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

// All inclusion listing is dictionary order; Ordering is case insensitive.
// Standard C/C++ library & boost library #includes

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
#include <codecvt>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#define BOOST_NO_AUTO_PTR

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/date_time/c_time.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/program_options.hpp>
#include <boost/range/any_range.hpp>
#include <boost/regex.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/signals2.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>

using std::max; using std::min;

// External library #includes

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <http-parser/http_parser.h>
#include <openssl/sha.h>
#include <openssl/tls1.h>

#include "RapidJson.h"

extern "C"
{
#include <scrypt/lib/crypto/crypto_scrypt.h>
}

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4819) // warning C4819: The file contains a character that cannot be represented in the current code page. Save the file in Unicode format to prevent data loss
#endif
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#ifdef _MSC_VER
#    pragma warning(pop) // warning C4819: The file contains a character that cannot be represented in the current code page. Save the file in Unicode format to prevent data loss
#endif

#ifdef _MSC_VER
#    pragma warning(push)
#    pragma warning(disable : 4819) // warning C4819: The file contains a character that cannot be represented in the current code page. Save the file in Unicode format to prevent data loss
#endif
#include <SQLiteCpp/SQLiteCpp.h>
#ifdef _MSC_VER
#    pragma warning(pop)
#endif
#include <SQLiteCpp/sqlite3/sqlite3.h>

#include <zlib.h>

// Other common #includes

#include "Types.h"
