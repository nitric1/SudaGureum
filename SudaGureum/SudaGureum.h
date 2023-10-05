#pragma once

namespace SudaGureum
{
#ifdef _WIN32
#  define NATIVE_MAIN_NAME wmain
#else
#  define NATIVE_MAIN_NAME main
#endif
}
