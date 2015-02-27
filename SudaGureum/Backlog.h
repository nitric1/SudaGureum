#pragma once

namespace SudaGureum
{
    class Backlog
    {
    private:
        struct Line
        {
            size_t startPos_;
            size_t size_;
            boost::posix_time::ptime time_;
        };

    public:
        Backlog();

    private:
        std::string buffer_;
        std::deque<Line> log_;
    };
}
