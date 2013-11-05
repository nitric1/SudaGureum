#include "Common.h"

#include "SudaGureum.h"

#include "IrcClient.h"

namespace SudaGureum
{
    void run()
    {
        IrcClientPool pool;

        auto client1 = pool.connect("irc.ozinger.org", 6667, boost::assign::list_of("SudaGureum1")("SudaGureum2"));

        pool.run(4);

        Sleep(100);

        auto client2 = pool.connect("irc.ozinger.org", 6667, boost::assign::list_of("SudaGureum1")("SudaGureum2"));

        pool.join();
    }
}

int main()
{
    SudaGureum::run();

    return 0;
}
