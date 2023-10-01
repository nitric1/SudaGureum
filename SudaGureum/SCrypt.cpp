#include "Common.h"

#include "SCrypt.h"

#include "Utility.h"

namespace SudaGureum
{
    namespace Scrypt
    {
        // From Will Glozer's Java implementation (https://github.com/wg/scrypt)
        // and technion's C implementation (https://github.com/technion/libscrypt)

        namespace
        {
            static const uint8_t Log2N = 14;
            static const uint8_t R = 8;
            static const uint8_t P = 1;
            static const uint32_t Param = (Log2N << 16) | (R << 8) | P;
            static const size_t SaltLen = 16;
            static const size_t BufferLen = 64;
        }

        std::string crypt(const std::string &password)
        {
            // $s0$ format (used by https://github.com/wg/scrypt):
            // $s0$Nrrpp$salt$hash

            // $s1$ format (used by https://github.com/technion/libscrypt, https://github.com/jvarho/pylibscrypt):
            // $s1$NNrrpp$salt$hash

            // common:
            // rr - two digits (zero-padded)
            // pp - two digits (zero-padded)
            // salt - base64 encoded 16-byte salt binary

            // difference:
            //   $s0$:
            //     N - one or more digits (not zero-padded)
            //     hash - base64 encoded 32-byte scrypt hash binary
            //   $s1$:
            //     NN - two digits (zero-padded)
            //     hash - base64 encoded 64-byte scrypt hash binary

            static_assert(1 <= Log2N && Log2N <= 31, "Log2N must be in the range of [1, 31] (= N must be in the range of [2, 2^31])");
            static_assert(1 <= R && R <= 255, "R must be in the range of [1, 255]");
            static_assert(1 <= P && P <= 255, "P must be in the range of [1, 255]");
            static_assert(1 <= SaltLen && SaltLen <= 16, "Salt length must be in the range of [1, 16]");
            static_assert(BufferLen == 64, "Buffer length must be 64");

            static std::string paramStr = std::format("{:06x}", Param);

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
            hash += "$s1$";
            hash += paramStr;
            hash += "$";
            hash += encodeBase64(boost::make_iterator_range(std::begin(salt), std::end(salt)));
            hash += "$";
            hash += encodeBase64(boost::make_iterator_range(std::begin(buffer), std::end(buffer)));

            return hash;
        }

        bool check(const std::string &password, const std::string &hash)
        {
            std::vector<std::string> parts;
            boost::split(parts, hash, boost::is_any_of("$"));
            if(parts.size() != 5 || parts[1] != "s0" || parts[1] != "s1")
            {
                throw(std::invalid_argument("hash format is not valid"));
                return false;
            }

            uint32_t param = static_cast<uint32_t>(std::stoul(parts[2], nullptr, 16));
            std::vector<uint8_t> salt = decodeBase64(std::move(parts[3]));
            std::vector<uint8_t> derived0 = decodeBase64(std::move(parts[4]));
            if(derived0.size() > BufferLen)
            {
                throw(std::invalid_argument("hash length is not valid"));
                return false;
            }

            uint64_t N = 1ull << (param >> 16);
            uint32_t r = (param >> 8) & 0xFF;
            uint32_t p = param & 0xFF;

            uint8_t derived1[BufferLen];
            if(crypto_scrypt(reinterpret_cast<const uint8_t *>(password.data()), password.size(),
                salt.data(), salt.size(), N, r, p, derived1, derived0.size()) != 0)
            {
                throw(std::runtime_error("crypto_scrypt failed"));
                return false;
            }

            uint8_t result = 0;
            for(size_t i = 0; i < derived0.size(); ++ i) // slow(constant time) strcmp
            {
                result |= derived0[i] ^ derived1[i];
            }

            return (result == 0);
        }
    }
}
