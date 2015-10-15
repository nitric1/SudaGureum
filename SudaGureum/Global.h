#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    class GlobalContext : public Singleton<GlobalContext>
    {
    private:
        GlobalContext();

    public:
        bool daemonized() const { return daemonized_; }

    private:
        bool daemonized_;

        friend class Singleton<GlobalContext>;
        friend void run(int argc, wchar_t **argv);
        friend void run(int argc, char **argv);
    };
}
