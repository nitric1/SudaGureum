#include "Common.h"

#include "Thread.h"

namespace Batang
{
    void TaskPool::push(const std::function<void ()> &task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(task);
    }

    std::function<void ()> TaskPool::pop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::function<void ()> task = std::move(queue_.front());
        queue_.pop_front();
        return task;
    }

    bool TaskPool::empty()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    namespace
    {
        void doNothing(ThreadTaskPool *) {}
    }

    boost::thread_specific_ptr<ThreadTaskPool> ThreadTaskPool::currentTaskPool_(doNothing);

    ThreadTaskPool *ThreadTaskPool::getCurrent()
    {
        return currentTaskPool_.get();
    }

    void ThreadTaskPool::setCurrent(ThreadTaskPool *current)
    {
        currentTaskPool_.reset(current);
    }

    ThreadTaskPool::ThreadTaskPool()
    {
        invokedLock_ = CreateSemaphoreW(nullptr, 0, 1, nullptr);
    }

    void ThreadTaskPool::invoke(const std::function<void ()> &task)
    {
        taskPool_.push(task);
        ReleaseSemaphore(invokedLock_, 1, nullptr);
        // TODO: wait for task executed
    }

    void ThreadTaskPool::post(const std::function<void ()> &task)
    {
        taskPool_.push(task);
        ReleaseSemaphore(invokedLock_, 1, nullptr);
    }

    void ThreadTaskPool::process()
    {
        WaitForSingleObject(invokedLock_, 0);
        while(!taskPool_.empty()) // This is OK because popping can be only in this class.
            taskPool_.pop()();
    }
}
