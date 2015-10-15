#pragma once

namespace SudaGureum
{
#ifdef _WIN32
    void run(int argc, wchar_t **argv);
#else
    void run(int argc, char **argv);
#endif
}
