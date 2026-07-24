#pragma once

#include <cstdint>

using b8  = bool;
using c8  = char;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = float;
using f64 = double;

#if !defined(EXCALIBUR_FORCE_INLINE)
    #if defined(_MSC_VER)
        #define EXCALIBUR_FORCE_INLINE __forceinline
    #elif defined(__GNUC__) || defined(__clang__)
        #define EXCALIBUR_FORCE_INLINE inline __attribute__((always_inline))
    #else
        #define EXCALIBUR_FORCE_INLINE inline
    #endif
#endif