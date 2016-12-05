#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    /*
     * Meaning of logger levels:
     *   trace: not harmful; should be ignored; just for lower-level contextual information, or really just for printing (data read/written, etc)
     *   debug: info's only for debug mode
     *   info: not harmful; just for higher-level contextual information (socket connection, HTTP request, etc)
     *   warn: recovable errors including wrong data input (= user failures)
     *   error: non-recovable errors
     *   critical: errors must be fixed first
     */

    // Logger wrapper class which exposes interface returning the `spdlog::logger` instance.
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
