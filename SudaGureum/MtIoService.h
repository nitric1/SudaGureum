#pragma once

namespace SudaGureum
{
    class MtIoService
    {
    private:
        static void runImpl(boost::asio::io_service &ios);

    public:
        virtual ~MtIoService() = 0;

    public:
        void run(uint16_t numThreads);
        void join();
        void stop();

    protected:
        boost::asio::io_service ios_;
        std::vector<std::shared_ptr<std::thread>> threads_;
    };
}
