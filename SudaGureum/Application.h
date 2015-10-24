#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    class Application : public Singleton<Application>
    {
    private:
        Application();

    public:
        int run(int argc, native_char_t **argv);

    public:
        bool daemonized() const { return daemonized_; }

    private:
        bool daemonized_;

        friend class Singleton<Application>;
    };
}
