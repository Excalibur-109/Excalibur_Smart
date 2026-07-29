#pragma once

#if !defined(MATH_DISABLE_SIMD) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__))
#include <immintrin.h>
#define MATH_HAS_SSE2 1
#else
#define MATH_HAS_SSE2 0
#endif