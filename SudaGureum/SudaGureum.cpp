#include "Common.h"

#include "SudaGureum.h"

#include "Configure.h"
#include "IrcClient.h"
#include "User.h"
#include "Utility.h"
#include "WebSocketServer.h"

namespace SudaGureum
{
#ifdef _WIN32
    void run(int argc, wchar_t **argv)
#else
    void run(int argc, char **argv)
#endif
    {
        try
        {
            namespace boostpo = boost::program_options;

#ifndef _WIN32
            boost::locale::generator gen;
            std::locale loc = gen("");
            std::locale::global(loc);
#endif

            boostpo::options_description desc("Generic options");
            desc.add_options()
                ("help,h", "show help message")
                ("version,V", "show version info")
                ("config", boostpo::wvalue<std::wstring>(), "specify configure file");

            boostpo::variables_map vm;
            try
            {
                boostpo::store(boostpo::parse_command_line(argc, argv, desc), vm);
                boostpo::notify(vm);
            }
            catch(boostpo::error &ex)
            {
                std::cerr << ex.what() << std::endl;
                return;
            }

            if(vm.find("help") != vm.end())
            {
                std::cout << desc << std::endl;
                return;
            }

            if(vm.find("config") != vm.end())
            {
                if(!Configure::instance().load(boost::filesystem::wpath(vm["config"].as<std::wstring>())))
                {
                    std::cerr << "Failed to load the configure file; ignore." << std::endl;
                }
            }

            IrcClientPool pool;
            WebSocketServer server(44444, true);

            // auto client1 = pool.connect("altirc.ozinger.org", 80, "UTF-8", {"SudaGureum1", "SudaGureum2"});
            Users::instance().load(pool);

            pool.run(4);
            server.run(4);

            // Sleep(100);

            // auto client2 = pool.connect("irc.ozinger.org", 6667, boost::assign::list_of("SudaGureum1")("SudaGureum2"));

            // pool.join();

            auto user = Users::instance().user("SudaGureum");
            auto servers = user->servers();
            auto client1 = servers.find("Ozinger")->second.ircClient_;

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
                size_t wlen = static_cast<size_t>(MultiByteToWideChar(CP_ACP, 0, line.c_str(), -1, nullptr, 0));
                std::vector<wchar_t> wbuf(wlen);
                MultiByteToWideChar(CP_ACP, 0, line.c_str(), -1, wbuf.data(), static_cast<int>(wlen));

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
        catch(boost::system::system_error &e)
        {
            std::cerr << "System error: " << e.what() << std::endl;
        }
        catch(std::runtime_error &e)
        {
            std::cerr << "Runtime error: " << e.what() << std::endl;
        }
        catch(std::exception &e)
        {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
        catch(...)
        {
            std::cerr << "Unexpected exception caught." << std::endl;
        }
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
#else
int main(int argc, char **argv)
#endif
{
    SudaGureum::run(argc, argv);

    return 0;
}
