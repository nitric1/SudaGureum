#include "Common.h"

#include "Application.h"

#include "Archive.h"
#include "Configure.h"
#include "Default.h"
#include "IrcClient.h"
#include "Log.h"
#include "SCrypt.h"
#include "User.h"
#include "Utility.h"
#include "HttpServer.h"

namespace SudaGureum
{
    Application::Application()
        : daemonized_(false)
    {
    }

    namespace
    {
        void setupLogger()
        {
            Log::instance();
        }

        std::string exceptionErrorMessage()
        {
            try
            {
                throw;
            }
            catch(const boost::system::system_error &e)
            {
                return fmt::format("System error: {}", e.what());
            }
            catch(const std::runtime_error &e)
            {
                return fmt::format("Runtime error: {}", e.what());
            }
            catch(const std::exception &e)
            {
                return fmt::format("Exception: {}", e.what());
            }
            catch(...)
            {
                return "Unexpected exception caught.";
            }

            return {};
        }
    }

    int Application::run(int argc, native_char_t **argv)
    {
        try // before logger setup
        {
            namespace boostpo = boost::program_options;

#ifndef _WIN32
            std::locale loc = boost::locale::generator()("");
            std::locale::global(loc);
#endif

            boostpo::options_description desc("Generic options");
            desc.add_options()
                ("help,h", "show help message")
                ("version,V", "show version info")
                ("config,c", boostpo::wvalue<std::wstring>(), "specify configure file")
#ifndef _WIN32
                ("daemon,d", "run as daemonized mode")
#endif
                ;

            boostpo::variables_map vm;
            try
            {
                boostpo::store(boostpo::parse_command_line(argc, argv, desc), vm);
                boostpo::notify(vm);
            }
            catch(const boostpo::error &ex)
            {
                std::cerr << ex.what() << std::endl;
                return EXIT_FAILURE;
            }

            if(vm.find("help") != vm.end())
            {
                std::cout << desc << std::endl;
                return EXIT_SUCCESS;
            }

            if(vm.find("config") != vm.end())
            {
                if(!Configure::instance().load(boost::filesystem::wpath(vm["config"].as<std::wstring>())))
                {
                    std::cerr << "Failed to load the configure file; ignore." << std::endl;
                }
            }

            daemonized_ = (vm.find("daemon") != vm.end());

            setupLogger();
        }
        catch(...)
        {
            std::cerr << exceptionErrorMessage() << std::endl;
            return EXIT_FAILURE;
        }

        try // after logger setup
        {
            IrcClientPool pool;
            HttpServer server(44444, true);
            registerHttpResourceProcessors(server);

            // auto client1 = pool.connect("altirc.ozinger.org", 80, "UTF-8", {"SudaGureum1", "SudaGureum2"});
            Users::instance().load(pool);

            pool.run(4);
            server.run(4);

            // Sleep(100);

            auto user = Users::instance().user("SudaGureum");
            auto servers = user->servers();
            auto client1 = servers.find("Ozinger")->ircClient_;

            while(std::cin)
            {
                std::string line;
                std::getline(std::cin, line);

                boost::algorithm::trim(line);

                if(line.empty())
                {
                    break;
                }

#ifdef _WIN32
                size_t wlen = static_cast<size_t>(MultiByteToWideChar(CP_ACP, 0,
                    line.c_str(), static_cast<int>(line.size() + 1), nullptr, 0));
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_ACP, 0,
                    line.c_str(), static_cast<int>(line.size() + 1), wbuf.data(), static_cast<int>(wlen));

                client1.lock()->privmsg("#HNO3", encodeUtf8(wbuf.data()));
#else
                client1.lock()->privmsg("#HNO3", boost::locale::conv::to_utf<char>(line, std::locale()));
#endif
            }

            pool.closeAll();
            pool.join();

            server.stop();
            server.join();
        }
        catch(...)
        {
            Log::instance().error(exceptionErrorMessage());
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }

    void Application::registerHttpResourceProcessors(HttpServer &server)
    {
        server.registerResourceProcessor("/",
            [](HttpConnection &conn, const HttpRequest &request, HttpResponse &response)
            {
                static const std::string Content = "Index page";
                response.body_.assign(Content.begin(), Content.end());
                return true;
            });

        server.registerResourceProcessor("/test",
            [](HttpConnection &conn, const HttpRequest &request, HttpResponse &response)
            {
                static const std::string Content = "Test page";
                response.body_.assign(Content.begin(), Content.end());
                return true;
            });
    }
}
