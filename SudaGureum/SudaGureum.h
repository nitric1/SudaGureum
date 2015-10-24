#pragma once

namespace SudaGureum
{
#ifdef _WIN32
#  define SUDAGUREUM_MAIN_NAME wmain
#else
#  define SUDAGUREUM_MAIN_NAME main
#endif
}
