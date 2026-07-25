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
    };
} // namespace Math