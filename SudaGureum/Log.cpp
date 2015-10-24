#include "Common.h"

#include "Log.h"

#include "Application.h"
#include "Configure.h"
#include "Default.h"

namespace SudaGureum
{
    namespace
    {
        class StdoutSink : public spdlog::sinks::base_sink<std::mutex>
        {
        public:
            StdoutSink() {}
            StdoutSink(const StdoutSink &) = delete;
            StdoutSink& operator =(const StdoutSink &) = delete;
            virtual ~StdoutSink() {}

        protected:
            void _sink_it(const spdlog::details::log_msg &msg) override
            {
#ifdef _WIN32
                size_t len = static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0,
                    msg.formatted.data(), static_cast<int>(msg.formatted.size()), nullptr, 0));
                std::vector<wchar_t> wbuf(len);
                MultiByteToWideChar(CP_UTF8, 0, msg.formatted.data(), static_cast<int>(msg.formatted.size()),
                    wbuf.data(), static_cast<int>(len));

                len = static_cast<size_t>(WideCharToMultiByte(CP_ACP, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                    nullptr, 0, nullptr, nullptr));
                std::vector<char> buf(len);
                WideCharToMultiByte(CP_ACP, 0, wbuf.data(), static_cast<int>(wbuf.size()),
                    buf.data(), static_cast<int>(len), nullptr, nullptr);
                std::cout.write(buf.data(), buf.size());
#else
                std::cout << boost::locale::conv::from_utf(msg.formatted.str(), std::locale());
#endif
                std::cout.flush();
            }

            void flush() override
            {
                std::cout.flush();
            }
        };
    }

    std::shared_ptr<spdlog::logger> Log::logger_;

    Log::Log()
    {
        spdlog::set_pattern("[%H:%M:%S.%e %z][%t] %v");

        boost::filesystem::path logPath = Configure::instance().get("log_path", DefaultConfigureValue::LogPath);
        boost::system::error_code ec;
        if(!boost::filesystem::create_directories(logPath, ec) && ec)
        {
            throw(std::runtime_error(fmt::format("cannot create data directory: {}", ec.message())));
            return;
        }

        std::vector<spdlog::sink_ptr> sinks =
        {
            std::make_shared<spdlog::sinks::daily_file_sink_mt>((logPath / "SudaGureum").string(), "log", 0, 0),
        };

        if(!Application::instance().daemonized())
        {
            sinks.push_back(std::make_shared<StdoutSink>());
        }

        logger_ = std::make_shared<spdlog::logger>("main", sinks.begin(), sinks.end());
        spdlog::register_logger(logger_);
    }

    spdlog::logger &Log::instance()
    {
        return *Singleton<Log>::instance().logger_;
    }
}
