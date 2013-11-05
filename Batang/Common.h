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

#include <cassert>

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <boost/thread/once.hpp>
#include <boost/thread/tss.hpp>
#include <boost/type_traits/function_traits.hpp>
#include <boost/filesystem.hpp>

using std::max; using std::min;

// External library inclusion

// Windows API inclusion; <windows.h> should be first.

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#pragma warning(push)
#pragma warning(disable: 4005) // macro redefinition, for INT8_MIN, INT8_MAX, ...
#include <intrin.h>
#include <windowsx.h>
#pragma warning(pop)

// Other common inclusion

#include "Types.h"
