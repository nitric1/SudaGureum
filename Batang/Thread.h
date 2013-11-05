#pragma once

namespace Batang
{
    class TaskPool
    {
    private:
        std::deque<std::function<void ()>> queue_;
        std::mutex mutex_;

    public:
        void push(const std::function<void ()> &task);
        std::function<void ()> pop();
        bool empty();
    };

    // TODO: move to detail
    class ThreadTaskPool
    {
    private:
        static boost::thread_specific_ptr<ThreadTaskPool> currentTaskPool_;
        TaskPool taskPool_;
        HANDLE invokedLock_;

    public:
        static ThreadTaskPool *getCurrent();

    protected:
        static void setCurrent(ThreadTaskPool *);

    public:
        ThreadTaskPool();

    public:
        virtual void invoke(const std::function<void ()> &task);
        virtual void post(const std::function<void ()> &task);

    protected:
        void process();

    public:
        HANDLE invokedLock() const { return invokedLock_; }
    };

    template<typename Derived>
    class Thread : public ThreadTaskPool, public std::enable_shared_from_this<Derived>
    {
    private:
        template<typename ...Args>
        struct RunReturnTypeDeduce
        {
            typedef typename std::result_of<decltype(&Derived::run)(Derived, Args...)>::type Type;
        };

    public:
        template<typename ...Args>
        void start(Args &&...args)
        {
            std::thread t(std::bind(&Thread::runImpl, shared_from_this(), std::forward<Args>(args)...));
            t.detach();
        }

        template<typename ...Args>
        typename RunReturnTypeDeduce<Args...>::Type runFromThisThread(Args &&...args)
        {
            return runImpl(std::forward<Args>(args)...);
        }

    private:
        template<typename ...Args>
        typename RunReturnTypeDeduce<Args...>::Type runImpl(Args &&...args)
        {
            setCurrent(this);
            auto ret = static_cast<Derived *>(this)->run(std::forward<Args>(args)...);
            setCurrent(nullptr);
            return ret;
        }
    };
}
