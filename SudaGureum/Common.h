#pragma once

// Definition for windows

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _VARIADIC_MAX 10
#define _SCL_SECURE_NO_WARNINGS

#define WINVER 0x0600 // Windows Vista
#define _WIN32_WINDOWS 0x0600
#define _WIN32_WINNT 0x0600
#define _WIN32_IE 0x0800
#define NTDDI_VERSION 0x06000100

// All inclusion listing is dictionary order; Ordering is case insensitive.
// Standard C/C++ library & boost library inclusion

// FIXME: g++ does not support <codecvt>. Use <codecvt> and remove <boost/locale.hpp> AFTER g++ supports it.

#include <algorithm>
#include <atomic>
#include <bitset>
#include <chrono>
// #include <codecvt>
#include <deque>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <byteswap.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/c_time.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>
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

// External library inclusion

#include <cppformat/format.h>
#include <http-parser/http_parser.h>
#include <openssl/sha.h>
#include <openssl/tls1.h>

#include "RapidJson.h"

extern "C"
{
#include <scrypt/lib/crypto/crypto_scrypt.h>
}

#include <spdlog/spdlog.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4819) // warning C4819: The file contains a character that cannot be represented in the current code page. Save the file in Unicode format to prevent data loss
#endif
#include <SQLiteCpp/SQLiteCpp.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <zlib-ng/zlib.h>

// Other common inclusion

#include "Types.h"
