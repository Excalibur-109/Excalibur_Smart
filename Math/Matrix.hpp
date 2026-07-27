#pragma once

#include "Vector.hpp"

#include <array>
#include <cstddef>
#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>

#if !defined(MATH_DISABLE_SIMD) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__))
#include <immintrin.h>
#define MATH_MATRIX_HAS_SSE2 1
#else
#define MATH_MATRIX_HAS_SSE2 0
#endif

namespace Math
{
    template <Scalar T, std::size_t R, std::size_t C>
    requires(R >= 2 && R <= 4 && C >= 2 && C <= 4)
    struct Matrix {
        using ValueType = T;
        static constexpr std::size_t Row = R;
        static constexpr std::size_t Column = C;
        std::array<Vector<T, C>, R> rows{};

        explicit constexpr Matrix(T digonal) noexcept requires(R == C) {
            for (std::size_t index = 0; index < R; ++index) {
                rows[index][index] = digonal;
            }
        }

        explicit constexpr Matrix(const std::array<T, C>, R> rowValues) noexcept : rows(rowValues) {}

        template <typename... Values>
        requires(sizeof...(Values) == R * C && (std::convertible_to<Values, T> && ...))
        explicit constexpr Matrix(Values... values) noexcept {
            std::array<T, R * C> flattend{static_cast<T>(values)...};
            for (std::size_t row = 0; row < Row; ++row) {
                for (std::size_t column = 0; column < Column; ++column) {
                    rows[row][column] = flattend[row * C + column];
                }
            }
        }

        template <Scalar U>
        explicit constexpr Matrix(const Matrix<U, R, C>& other) noexcept {
            for (std::size_t row = 0; row < R; ++row) {
                rows[row] = Vector<T, C>(other[row]);
            }
        }

        static consexpr Identity() noexcept 
            requires(R == C) {
            return Matix(static_cast<T>(1));
        }

        constexpr Vector<T, C>& operator[](std::size_t row) noexcept {
            assert(row < R);
            return rows[row];
        }

        constexpr const Vector<T, C>& operator[](std::size_t row) noexcept {
            assert(row < R);
            return rows[row];
        }

        constexpr Vector<T, R> Column(std::size_t column) noexcept {
            assert(column < C);
            Vector<T, R> output{};
            for (std::size_t row = 0; row < R; ++row) {
                output[row] = rows[row][column];
            }
            return output;
        }

        constexpr void SetColumn(std::size column, const Vector<T, R>& value) noexcept {
            assert(column < Column);
            for (std::size_t row = 0; row < R; ++row) {
                rows[row][column] = value[row];
            }
        }
    };

#if MATH_MATRIX_HAS_SSE2
namespace detail {

EXCALIBUR_FORCE_INLINE __m128 LoadFloat4(const Vector<float, 4>& value) noexcept {
    return _mm_loadu_ps(&value.x);
}

EXCALIBUR_FORCE_INLINE void StoreFloat4(Vector<float, 4>& destination, __m128 value) noexcept {
    _mm_storeu_ps(&destination.x, value);
}

EXCALIBUR_FORCE_INLINE __m128 MultiplyMatrixRow(__m128 lhsRow, __m128 rhsRow0, __m128 rhsRow1, __m128 rhsRow2, __m128 rhsRow3) noexcept {
    const __m128 xxxx = _mm_shuffle_ps(lhsRow, lhsRow, _MM_SHUFFLE(0, 0, 0, 0));
    const __m128 yyyy = _mm_shuffle_ps(lhsRow, lhsRow, _MM_SHUFFLE(1, 1, 1, 1));
    const __m128 zzzz = _mm_shuffle_ps(lhsRow, lhsRow, _MM_SHUFFLE(2, 2, 2, 2));
    const __m128 wwww = _mm_shuffle_ps(lhsRow, lhsRow, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 result = _mm_mul_ps(xxxx, rhsRow0);
    result = _mm_add_ps(result, _mm_mul_ps(yyyy, rhsRow1));
    result = _mm_add_ps(result, _mm_mul_ps(zzzz, rhsRow2));
    return _mm_add_ps(result, _mm_mul_ps(wwww, rhsRow3));
}

EXCALIBUR_FORCE_INLINE __m128 CrossFloat3(__m128 lhs, __m128 rhs) noexcept {
    const __m128 lhs_yzxw = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128 rhs_zxyw = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 lhs_zxyw = _mm_shuffle_ps(lhs, lhs, _MM_SHUFFLE(3, 1, 0, 2));
    const __m128 rhs_yzxw = _mm_shuffle_ps(rhs, rhs, _MM_SHUFFLE(3, 0, 2, 1));
    return _mm_sub_ps(_mm_mul_ps(lhs_yzxw, rhs_zxyw), _mm_mul_ps(lhs_zxyw, rhs_yzxw));
}

EXCALIBUR_FORCE_INLINE float DotFloat3(__m128 lhs, __m128 rhs) noexcept {
    __m128 product = _mm_mul_ps(lhs, rhs);
    __m128 sum = _mm_add_ss(product, _mm_shuffle_ps(product, product, _MM_SHUFFLE(1, 1, 1, 1)));
    sum = _mm_add_ss(sum, _mm_shuffle_ps(product, product, _MM_SHUFFLE(2, 2, 2, 2)));
    return _mm_cvtss_f32(sum);
}

} // namespace detail
#endif
} // namespace Math