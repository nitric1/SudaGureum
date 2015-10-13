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
    private:
        static void deleter(T *ptr)
        {
            delete ptr;
        }

    public:
        static T &instance()
        {
            static boost::thread_specific_ptr<T> ptr(deleter);
            if(ptr.get() == nullptr)
                ptr.reset(new T);
            return *ptr;
        }

    protected:
        ~SingletonLocal() {}
    };
}
