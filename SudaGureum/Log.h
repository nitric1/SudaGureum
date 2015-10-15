#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    class Log : public Singleton<Log>
    {
    public:
        static spdlog::logger &instance();

    private:
        static std::shared_ptr<spdlog::logger> logger_;

    private:
        Log();

        friend class Singleton<Log>;
    };
}
