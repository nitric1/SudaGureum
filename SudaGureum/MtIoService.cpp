#include "Common.h"

#include "MtIoService.h"

namespace SudaGureum
{
    MtIoService::~MtIoService()
    {
    }

    void MtIoService::run(uint16_t numThreads)
    {
        for(uint16_t i = 0; i < numThreads; ++ i)
        {
            std::shared_ptr<std::thread> thread(
                new std::thread(std::bind(
                    [](boost::asio::io_service &ios)
                    {
                        ios.run();
                    }, std::ref(ios_))));
            threads_.push_back(thread);
        }
    }

    void MtIoService::join()
    {
        for(auto &thread: threads_)
        {
            thread->join();
        }
    }

    void MtIoService::stop()
    {
        ios_.stop();
    }
}
