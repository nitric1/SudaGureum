#pragma once

namespace SudaGureum
{
    class MtIoService
    {
    private:
        static void runImpl(asio::io_context &ioc);

    public:
        virtual ~MtIoService() = 0;

    public:
        void run(uint16_t numThreads);
        void join();
        void stop();

    protected:
        asio::io_context ioc_;
        std::vector<std::shared_ptr<std::thread>> threads_;
    };
}
