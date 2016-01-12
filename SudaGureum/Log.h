#pragma once

#include "Singleton.h"

namespace SudaGureum
{
    /*
     * Meaning of logger levels:
     *   info: not harmful; just for lower-level contextual information, or really just for printing (socket connection etc...)
     *   trace: not harmful; just for higher-level contextual information (HTTP request etc...)
     *   debug: info's or traces only for debug mode
     *   alert: recovable errors including wrong data input (= user failures)
     *   error: non-recovable errors
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
