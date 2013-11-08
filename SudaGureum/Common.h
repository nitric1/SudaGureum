#pragma once

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

#include <atomic>
#include <bitset>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/regex.hpp>

using std::max; using std::min;

// External library inclusion

// Other common inclusion

#include "Batang/Types.h"
