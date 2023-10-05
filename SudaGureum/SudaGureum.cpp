#include "Common.h"

#include "SudaGureum.h"

#include "Application.h"

int NATIVE_MAIN_NAME(int argc, native_char_t **argv)
{
    return SudaGureum::Application::instance().run(argc, argv);
}
