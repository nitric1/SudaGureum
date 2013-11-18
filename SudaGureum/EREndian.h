#pragma once

/// An endian conversion functions' class.
/// @author Wondong LEE
class EREndian
{
private:
    /// Private and unimplemented constructor.
    /// @author Wondong LEE
    EREndian();

    /// Private and unimplemented destructor.
    /// @author Wondong LEE
    ~EREndian();

public:
    /// Swaps a 16-bit integer's lowest 8-bit and highest 8-bit.
    /// @author Wondong LEE
    /// @param v The value to swap.
    /// @return Swapped value.
    static uint16_t swap(uint16_t v)
    {
#if defined(_MSC_VER)
        return _byteswap_ushort(v);
#elif defined(__GNUC__)
        return bswap_16(v);
#else
        return (v << 8) | (v >> 8); // should be optimized
#endif
    }

    /// Swaps a 32-bit integer's lowest 8-bit and highest 8-bit and each of middle 8-bit's.
    /// @author Wondong LEE
    /// @param v The value to swap.
    /// @return Swapped value.
    static uint32_t swap(uint32_t v)
    {
#if defined(_MSC_VER)
        return _byteswap_ulong(v);
#elif defined(__GNUC__)
        return bswap_32(v);
#else
        return (v << 24) | ((v & 0xFF00u) << 8) | ((v & 0xFF0000u) >> 8) | (v >> 24);
#endif
    }

    /// Swaps a 64-bit integer's each low and high 8-bit's.
    /// @author Wondong LEE
    /// @param v The value to swap.
    /// @return Swapped value.
    static uint64_t swap(uint64_t v)
    {
#if defined(_MSC_VER)
        return _byteswap_uint64(v);
#elif defined(__GNUC__)
        return bswap_64(v);
#else
        return (v << 56) | ((v & 0xFF00ull) << 40) | ((v & 0xFF0000ull) << 24) |
            ((v & 0xFF000000ull) << 8) | ((v & 0xFF00000000ull) >> 8) |
            ((v & 0xFF0000000000ull) >> 24) | ((v & 0xFF000000000000ull) >> 40) |
            (v >> 56);
#endif
    }

#ifdef BOOST_BIG_ENDIAN
    /// Converts a 16-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t N2L(uint16_t v) { return swap(v); }
    
    /// Converts a 32-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t N2L(uint32_t v) { return swap(v); }

    /// Converts a 64-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t N2L(uint64_t v) { return swap(v); }
    
    /// Converts a 16-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t N2B(uint16_t v) { return v; }

    /// Converts a 32-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t N2B(uint32_t v) { return v; }

    /// Converts a 64-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t N2B(uint64_t v) { return v; }
#else
    /// Converts a 16-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t N2L(uint16_t v) { return v; }

    /// Converts a 32-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t N2L(uint32_t v) { return v; }

    /// Converts a 64-bit unsigned integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t N2L(uint64_t v) { return v; }

    /// Converts a 16-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t N2B(uint16_t v) { return swap(v); }

    /// Converts a 32-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t N2B(uint32_t v) { return swap(v); }

    /// Converts a 64-bit unsigned integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t N2B(uint64_t v) { return swap(v); }
#endif
    /// Converts a 16-bit unsigned integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t L2N(uint16_t v) { return N2L(v); }

    /// Converts a 32-bit unsigned integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t L2N(uint32_t v) { return N2L(v); }

    /// Converts a 64-bit unsigned integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t L2N(uint64_t v) { return N2L(v); }

    /// Converts a 16-bit unsigned integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint16_t B2N(uint16_t v) { return N2B(v); }

    /// Converts a 32-bit unsigned integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint32_t B2N(uint32_t v) { return N2B(v); }

    /// Converts a 64-bit unsigned integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static uint64_t B2N(uint64_t v) { return N2B(v); }

    /// Converts a 16-bit signed integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int16_t N2L(int16_t v) { return static_cast<int16_t>(N2L(static_cast<uint16_t>(v))); }

    /// Converts a 32-bit signed integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int32_t N2L(int32_t v) { return static_cast<int32_t>(N2L(static_cast<uint32_t>(v))); }

    /// Converts a 64-bit signed integer from native endian to little endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int64_t N2L(int64_t v) { return static_cast<int64_t>(N2L(static_cast<uint64_t>(v))); }

    /// Converts a 16-bit signed integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int16_t N2B(int16_t v) { return static_cast<int16_t>(N2B(static_cast<uint16_t>(v))); }

    /// Converts a 32-bit signed integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int32_t N2B(int32_t v) { return static_cast<int32_t>(N2B(static_cast<uint32_t>(v))); }

    /// Converts a 64-bit signed integer from native endian to big endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int64_t N2B(int64_t v) { return static_cast<int64_t>(N2B(static_cast<uint64_t>(v))); }

    /// Converts a 16-bit signed integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int16_t L2N(int16_t v) { return static_cast<int16_t>(L2N(static_cast<uint16_t>(v))); }

    /// Converts a 32-bit signed integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int32_t L2N(int32_t v) { return static_cast<int32_t>(L2N(static_cast<uint32_t>(v))); }

    /// Converts a 64-bit signed integer from little endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int64_t L2N(int64_t v) { return static_cast<int64_t>(L2N(static_cast<uint64_t>(v))); }

    /// Converts a 16-bit signed integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int16_t B2N(int16_t v) { return static_cast<int16_t>(B2N(static_cast<uint16_t>(v))); }

    /// Converts a 32-bit signed integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int32_t B2N(int32_t v) { return static_cast<int32_t>(B2N(static_cast<uint32_t>(v))); }

    /// Converts a 64-bit signed integer from big endian to native endian.
    /// @author Wondong LEE
    /// @param v The value to convert.
    /// @return Converted value.
    static int64_t B2N(int64_t v) { return static_cast<int64_t>(B2N(static_cast<uint64_t>(v))); }
};
