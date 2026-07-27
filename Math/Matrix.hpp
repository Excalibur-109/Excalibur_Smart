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

        constexpr Matrix() noexcept = default;

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

        constexpr void SetColumn(std::size_t column, const Vector<T, R>& value) noexcept {
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

EXCALIBUR_FORCE_INLINE bool TryInverseFloat4x4(const Matrix<float, 4, 4>& matrix, Matrix<float, 4, 4>* output, float epsilon) noexcept {
    __m128 a = LoadFloat4(matrix.rows[0]);
    __m128 b = LoadFloat4(matrix.rows[1]);
    __m128 c = LoadFloat4(matrix.rows[2]);
    __m128 d = LoadFloat4(matrix.rows[3]);
    __m128 aw = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 bw = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 cw = _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 dw = _mm_shuffle_ps(d, d, _MM_SHUFFLE(3, 3, 3, 3));

    __m128 s = CrossFloat3(a, b);
    __m128 t = CrossFloat3(c, d);
    __m128 u = _mm_sub_ps(_mm_mul_ps(a, bw), _mm_mul_ps(b, aw));
    __m128 v = _mm_sub_ps(_mm_mul_ps(c, dw), _mm_mul_ps(d, cw));
    const float determinant = DotFloat3(s, v) + DotFloat3(t, u);
    if (Abs(determinant) <= epsilon) {
        return false;
    }

    __m128 inverseDeterminant = _mm_set1_ps(1.0F / determinant);
    s = _mm_mul_ps(s, inverseDeterminant);
    t = _mm_mul_ps(t, inverseDeterminant);
    u = _mm_mul_ps(u, inverseDeterminant);
    v = _mm_mul_ps(v, inverseDeterminant);
    const __m128 row0 = _mm_add_ps(CrossFloat3(b, v), _mm_mul_ps(t, bw));
    const __m128 row1 = _mm_sub_ps(CrossFloat3(v, a), _mm_mul_ps(t, aw));
    const __m128 row2 = _mm_add_ps(CrossFloat3(d, u), _mm_mul_ps(s, dw));
    const __m128 row3 = _mm_sub_ps(CrossFloat3(u, c), _mm_mul_ps(s, cw));
    StoreFloat4(output->rows[0], row0);
    StoreFloat4(output->rows[1], row1);
    StoreFloat4(output->rows[2], row2);
    StoreFloat4(output->rows[3], row3);
    output->rows[0].w = -DotFloat3(b, t);
    output->rows[1].w = DotFloat3(a, t);
    output->rows[2].w = -DotFloat3(d, s);
    output->rows[3].w = DotFloat3(c, s);
    __m128 outputRow0 = LoadFloat4(output->rows[0]);
    __m128 outputRow1 = LoadFloat4(output->rows[1]);
    __m128 outputRow2 = LoadFloat4(output->rows[2]);
    __m128 outputRow3 = LoadFloat4(output->rows[3]);
    _MM_TRANSPOSE4_PS(outputRow0, outputRow1, outputRow2, outputRow3);
    StoreFloat4(output->rows[0], outputRow0);
    StoreFloat4(output->rows[1], outputRow1);
    StoreFloat4(output->rows[2], outputRow2);
    StoreFloat4(output->rows[3], outputRow3);
    return true;
}

EXCALIBUR_FORCE_INLINE Vector<float, 4> MultiplyFloat4x4Vector(const Matrix<float, 4, 4>& matrix, const Vector<float, 4>& vector) noexcept {
    __m128 column0 = LoadFloat4(matrix.rows[0]);
    __m128 column1 = LoadFloat4(matrix.rows[1]);
    __m128 column2 = LoadFloat4(matrix.rows[2]);
    __m128 column3 = LoadFloat4(matrix.rows[3]);
    _MM_TRANSPOSE4_PS(column0, column1, column2, column3);
    __m128 input = LoadFloat4(vector);
    __m128 xxxx = _mm_shuffle_ps(input, input, _MM_SHUFFLE(0, 0, 0, 0));
    __m128 yyyy = _mm_shuffle_ps(input, input, _MM_SHUFFLE(1, 1, 1, 1));
    __m128 zzzz = _mm_shuffle_ps(input, input, _MM_SHUFFLE(2, 2, 2, 2));
    __m128 wwww = _mm_shuffle_ps(input, input, _MM_SHUFFLE(3, 3, 3, 3));
    __m128 result = _mm_mul_ps(column0, xxxx);
    result = _mm_add_ps(result, _mm_mul_ps(column1, yyyy));
    result = _mm_add_ps(result, _mm_mul_ps(column2, zzzz));
    result = _mm_add_ps(result, _mm_mul_ps(column3, wwww));
    Vector<float, 4> output{};
    StoreFloat4(output, result);
    return output;
}

EXCALIBUR_FORCE_INLINE Matrix<float, 4, 4> MultiplyFloat4x4(const Matrix<float, 4, 4>& lhs, const Matrix<float, 4, 4>& rhs) noexcept {
    __m128 rhsRow0 = LoadFloat4(rhs.rows[0]);
    __m128 rhsRow1 = LoadFloat4(rhs.rows[1]);
    __m128 rhsRow2 = LoadFloat4(rhs.rows[2]);
    __m128 rhsRow3 = LoadFloat4(rhs.rows[3]);
    Matrix<float, 4, 4> output{};
    StoreFloat4(output.rows[0], MultiplyMatrixRow(LoadFloat4(lhs.rows[0]), rhsRow0, rhsRow1, rhsRow2, rhsRow3));    
    StoreFloat4(output.rows[1], MultiplyMatrixRow(LoadFloat4(lhs.rows[1]), rhsRow0, rhsRow1, rhsRow2, rhsRow3));
    StoreFloat4(output.rows[2], MultiplyMatrixRow(LoadFloat4(lhs.rows[2]), rhsRow0, rhsRow1, rhsRow2, rhsRow3));
    StoreFloat4(output.rows[3], MultiplyMatrixRow(LoadFloat4(lhs.rows[3]), rhsRow0, rhsRow1, rhsRow2, rhsRow3));
    return output;
}

} // namespace detail
#endif

template <Scalar T, std::size_t R, std::size_t C>
constexpr bool operator==(const Matrix<T, R, C>& lhs, const Matrix<T, R, C>& rhs) noexcept {
    for (std::size_t row = 0; row < R; ++row) {
        if (lhs[row] != rhs[row]) {
            return false;
        }
    }
    return true;
}

template <Scalar T, std::size_t R, std::size_t C>
constexpr bool operator!=(const Matrix<T, R, C>& lhs, const Matrix<T, R, C>& rhs) noexcept {
    return !(lhs == rhs);
}

#define DEFINE_MATRIX_BINARY_OPERATOR(OPERATOR) \
    template <ArithmeticScalar Lhs, ArithmeticScalar Rhs, std::size_t R, std::size_t C> \
    constexpr auto operator OPERATOR (const Matrix<Lhs, R, C>& lhs, const Matrix<Rhs, R, C> rhs) noexcept { \
        using Result = std::common_type_t<Lhs, Rhs>; \
        Matrix<Result, R, C> output{}; \
        for (std::size_t row = 0; row < R; ++row) { \
            output[row] = lhs[row] + rhs[row]; \
        } \
        return output; \
    }

DEFINE_MATRIX_BINARY_OPERATOR(+)
DEFINE_MATRIX_BINARY_OPERATOR(-)

#undef DEFINE_MATRIX_BINARY_OPERATOR

template <ArithmeticScalar T, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator-(const Matrix<T, R, C>& value) noexcept {
    Matrix<T, R, C> output{};
    for (std::size_t row = 0; row < R; ++row) {
        output[row] = -value[row];
    }
    return output;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr auto operator*(const Matrix<T, R, C>& matrix, U scalar) noexcept {
    using Result = std::common_type_t<T, U>;
    Matrix<Result, R, C> output{};
    for (std::size_t row = 0; row < R, ++row) {
        output[row] = matrix[row] * scalar;
    }
    return output;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr auto operator*(U scalar, const Matrix<T, R, C>& matrix) noexcept {
    return matrix * scalar;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr auto operator/(const Matrix<T, R, C>& matrix, U scalar) noexcept {
    using Result = std::common_type_t<T, U>;
    Matrix<Result, R, C> output{};
    for (std::size_t row = 0; row < R, ++row) {
        output[row] = matrix[row] / scalar;
    }
    return output;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator+=(Matrix<T, R, C>& lhs, const Matrix<U, R, C> rhs) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        lhs[row] += rhs[row];
    }
    return lhs;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator-=(Matrix<T, R, C>& lhs, const Matrix<U, R, C> rhs) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        lhs[row] -= rhs[row];
    }
    return lhs;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator*=(const Matrix<T, R, C>& matrix, U scalar) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        matrix[row] *= scalar;
    }
    return matrix;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator/=(const Matrix<T, R, C>& matrix, U scalar) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        matrix[row] /= scalar;
    }
    return matrix;
}

} // namespace Math