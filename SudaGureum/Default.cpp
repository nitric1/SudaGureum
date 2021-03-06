#include "Common.h"

#include "Default.h"

namespace SudaGureum
{
    namespace DefaultConfigureValue
    {
        const char *DataPath = "./Data";
        const char *LogPath = "./Data/Log";
        const long IrcClientCloseTimeoutSec = 5;
        const long WebSocketServerCloseTimeoutSec = 5;
        const long HttpServerKeepAliveTimeoutSec = 5;
        const size_t HttpServerKeepAliveMaxCount = 20;
    }
}
