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
            StdoutSink &operator=(const StdoutSink &) = delete;
            virtual ~StdoutSink() {}

        protected:
            void sink_it_(const spdlog::details::log_msg &msg) override
            {
                spdlog::memory_buf_t formatted;
                formatter_->format(msg, formatted);

#ifdef _WIN32
                size_t len = static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, formatted.data(), static_cast<int>(formatted.size()), nullptr, 0));
                std::vector<wchar_t> wbuf(len);
                MultiByteToWideChar(CP_UTF8, 0, formatted.data(), static_cast<int>(formatted.size()), wbuf.data(), static_cast<int>(len));

                len = static_cast<size_t>(WideCharToMultiByte(CP_ACP, 0, wbuf.data(), static_cast<int>(wbuf.size()), nullptr, 0, nullptr, nullptr));
                std::vector<char> buf(len);
                WideCharToMultiByte(CP_ACP, 0, wbuf.data(), static_cast<int>(wbuf.size()), buf.data(), static_cast<int>(len), nullptr, nullptr);
                std::cout.write(buf.data(), buf.size());
#else
                std::cout << boost::locale::conv::from_utf(formatted.str(), std::locale());
#endif
                std::cout.flush();
            }

            void flush_() override
            {
                std::cout.flush();
            }
        };
    }

    std::shared_ptr<spdlog::logger> Log::logger_;

    Log::Log()
    {
        spdlog::set_pattern("[%H:%M:%S.%e %z][%t] %v");

        std::filesystem::path logPath = Configure::instance().get("log_path").value_or(DefaultConfigureValue::LogPath);
        std::error_code ec;
        if(!std::filesystem::create_directories(logPath, ec) && ec)
        {
            throw(std::runtime_error(fmt::format("cannot create data directory: {}", ec.message())));
            return;
        }

        std::vector<spdlog::sink_ptr> sinks =
            {
                std::make_shared<spdlog::sinks::daily_file_sink_mt>((logPath / "SudaGureum").string(), 0, 0),
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
