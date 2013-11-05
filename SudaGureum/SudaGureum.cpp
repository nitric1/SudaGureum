#include "Common.h"

#include "SudaGureum.h"

#include "IrcParser.h"

void proc(const SudaGureum::IrcMessage &message)
{
    std::cout << message.prefix_ << std::endl;
    std::cout << message.command_ << std::endl;
    for(const std::string &param: message.params_)
    {
        std::cout << param << std::endl;
    }
}

int main()
{
    SudaGureum::IrcParser parser;

    parser.parse("PING :13722", &proc);
    parser.parse("11301\r\n", &proc);
    // parser.parse("", &proc);

    return 0;
}
