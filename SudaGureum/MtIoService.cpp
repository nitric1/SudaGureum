#include "Common.h"

#include "MtIoService.h"

namespace SudaGureum
{
    void MtIoService::runImpl(boost::asio::io_service &ios)
    {
        try
        {
            ios.run();
        }
        catch(const boost::system::system_error &e)
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
                new std::thread(std::bind(&MtIoService::runImpl, std::ref(ios_))));
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
