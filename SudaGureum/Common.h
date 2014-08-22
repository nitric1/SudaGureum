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
// #include <codecvt>
#include <deque>
#include <fstream>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <boost/algorithm/string.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/detail/endian.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem/path.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4819) // boost/format/alt_sstream_impl.hpp : warning C4819: The file contains a character that cannot be represented in the current code page
#endif
#include <boost/format.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <boost/lexical_cast.hpp>
#include <boost/locale.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/once.hpp>
#include <boost/uuid/sha1.hpp>

using std::max; using std::min;

// External library inclusion

#include <openssl/tls1.h>
#include <rapidjson/document.h>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__)
#include <byteswap.h>
#endif

// Other common inclusion

#include "Types.h"
