#pragma once

namespace SudaGureum
{
    template<typename T>
    class Singleton // thread-safe global
    {
    public:
        static T &instance()
        {
            static T inst;
            return inst;
        }

    protected:
        ~Singleton() {}
    };

    template<typename T>
    class SingletonLocal // thread-local (using TLS)
    {
    public:
        static T &instance()
        {
            thread_local static T inst;
            return inst;
        }

    protected:
        ~SingletonLocal() {}
    };
}
