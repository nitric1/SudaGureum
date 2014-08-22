#include "Common.h"

#include "SCrypt.h"

#include "Utility.h"

namespace SudaGureum
{
    // From Will Glozer's Java implementation (https://github.com/wg/scrypt)

    namespace
    {
        static const uint8_t Log2N = 14;
        static const uint32_t R = 8;
        static const uint32_t P = 1;
        static const uint32_t Param = (Log2N << 16) | (R << 8) | P;
        static const size_t SaltLen = 16;
        static const size_t BufferLen = 32;
    }

    std::string scryptPassword(const std::string &password)
    {
        static boost::once_flag paramStrOnceFlag = BOOST_ONCE_INIT;
        static std::string paramStr;
        boost::call_once(
            []()
            {
                std::ostringstream os;
                os << std::hex << Param;
                paramStr = os.str();
            }, paramStrOnceFlag);

        uint8_t salt[SaltLen];

        std::random_device rd;
        std::random_device::result_type *p = reinterpret_cast<std::random_device::result_type *>(salt);
        for(size_t i = 0; i < sizeof(salt) / sizeof(std::random_device::result_type); ++ i, ++ p)
        {
            *p = rd();
        }

        uint8_t buffer[BufferLen];
        if(crypto_scrypt(reinterpret_cast<const uint8_t *>(password.data()), password.size(),
            salt, sizeof(salt) / sizeof(*salt), 1ull << Log2N, R, P, buffer, sizeof(buffer) / sizeof(*buffer)) != 0)
        {
            return std::string();
        }

        std::string hash;
        hash.reserve(((sizeof(salt) / sizeof(*salt)) * 4 / 3 + (sizeof(buffer) / sizeof(*buffer)) * 4 / 3) * 2);
        hash += "$s0$";
        hash += paramStr;
        hash += "$";
        hash += encodeBase64(boost::make_iterator_range(std::begin(salt), std::end(salt)));
        hash += "$";
        hash += encodeBase64(boost::make_iterator_range(std::begin(buffer), std::end(buffer)));

        return hash;
    }

    bool scryptCheck(const std::string &password, const std::string &hash)
    {
        std::vector<std::string> parts;
        boost::split(parts, hash, boost::is_any_of("$"));
        if(parts.size() != 5 || parts[1] != "s0")
        {
            return false;
        }

        uint32_t param = static_cast<uint32_t>(strtoul(parts[2].c_str(), nullptr, 16));
        std::vector<uint8_t> salt = decodeBase64(std::move(parts[3]));
        std::vector<uint8_t> derived0 = decodeBase64(std::move(parts[4]));
        if(derived0.size() != BufferLen)
        {
            return false;
        }

        uint64_t N = 1 << (param >> 16);
        uint32_t r = (param >> 8) & 0xFF;
        uint32_t p = param & 0xFF;

        uint8_t derived1[BufferLen];
        if(crypto_scrypt(reinterpret_cast<const uint8_t *>(password.data()), password.size(),
            salt.data(), salt.size(), N, r, p, derived1, sizeof(derived1) / sizeof(*derived1)) != 0)
        {
            return false;
        }

        uint8_t result = 0;
        for(size_t i = 0; i < BufferLen; ++ i)
        {
            result |= derived0[i] ^ derived1[i];
        }

        return (result == 0);
    }
}
