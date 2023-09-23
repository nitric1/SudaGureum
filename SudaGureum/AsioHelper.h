#pragma once

namespace SudaGureum
{
    namespace StdAsioPlaceholders
    {
        static auto error = std::placeholders::_1;
        static auto bytesTransferred = std::placeholders::_2;
    }
}
