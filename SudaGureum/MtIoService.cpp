#include "Common.h"

#include "MtIoService.h"

namespace SudaGureum
{
    void MtIoService::runImpl(asio::io_context &ioc)
    {
        try
        {
            ioc.run();
        }
        catch(const std::system_error &e)
        {
            std::cerr << "System error: " << e.what() << std::endl;
        }
        catch(const std::runtime_error &e)
        {
            std::cerr << "Runtime error: " << e.what() << std::endl;
        }
        catch(const std::exception &e)
        {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
        catch(...)
        {
            std::cerr << "Unexpected exception caught." << std::endl;
        }
    }

    MtIoService::~MtIoService()
    {
    }

    void MtIoService::run(uint16_t numThreads)
    {
        for(uint16_t i = 0; i < numThreads; ++ i)
        {
            std::shared_ptr<std::thread> thread(
                new std::thread(std::bind(&MtIoService::runImpl, std::ref(ioc_))));
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
        ioc_.stop();
    }
}
