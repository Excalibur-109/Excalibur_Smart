#pragma once

#include "Vector.hpp"
#include "Define.hpp"

#include <array>
#include <cstddef>
#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>

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

#if MATH_HAS_SSE2
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
constexpr Matrix<T, R, C> operator*=(const Matrix<T, R, C>& lhs, U scalar) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        lhs[row] *= scalar;
    }
    return lhs;
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> operator/=(const Matrix<T, R, C>& lhs, U scalar) noexcept {
    for (std::size_t row = 0; row < R, ++row) {
        lhs[row] /= scalar;
    }
    return lhs;
}

template <ArithmeticScalar Lhs, ArithmeticScalar Rhs, std::size_t R, std::size_t C>
constexpr auto operator*(const Matrix<Lhs, R, C>& lhs, const Vector<Rhs, C>& rhs) noexcept {
    using Result = std::common_type_t<Lhs, Rhs>;
    Vector<Result, C> output{};
    for (std::size_t row = 0; row < R; ++row) {
        output[row] = Dot(lhs[row], rhs);
    }
    return output;
}

template <ArithmeticScalar Lhs, ArithmeticScalar Rhs, std::size_t R, std::size_t Shared, std::size_t C>
constexpr auto operator*(const Matrix<Lhs, R, Shared>& lhs, const Matrix<Rhs, Shared, C>& rhs) noexcept {
    using Result = std::common_type_t<Lhs, Rhs>;
    Matrix<Rhs, C, Shared> rhsColumns{};
    for (std::size_t column = 0; column < C; ++column) {
        for (std::size_t shared = 0; shared < Shared; ++shared) {
            rhsColumns.rows[column][shared] = rhs.rows[shared][column];
        }
    }
    Matrix<Result, R, C> output{};
    for (std::size_t row = 0; row < R, ++row) {
        for (std::size_t column = 0; column < C; ++column) {
            output.rows[row][column] = Dot(lhs.rows[row], rhsColumns.rows[column]);
        }
    }
    return output;
}

EXCALIBUR_FORCE_INLINE constexpr Vector<float, 4> operator*(const Matrix<float, 4, 4>& matrix, const Vector<float, 4>& vector) noexcept {
#if MATH_HAS_SSE2
    if (!std::is_constant_evaluated()) {
        return detail::MultiplyFloat4x4Vector(matrix, vector);
    }
#endif
    return {
        matrix.rows[0].x * vector.x + matrix.rows[0].y * vector.y + matrix.rows[0].z * vector.z + matrix.rows[0].w * vector.w,
        matrix.rows[1].x * vector.x + matrix.rows[1].y * vector.y + matrix.rows[1].z * vector.z + matrix.rows[1].w * vector.w,
        matrix.rows[2].x * vector.x + matrix.rows[2].y * vector.y + matrix.rows[2].z * vector.z + matrix.rows[2].w * vector.w,
        matrix.rows[3].x * vector.x + matrix.rows[3].y * vector.y + matrix.rows[3].z * vector.z + matrix.rows[3].w * vector.w,
    };
}

EXCALIBUR_FORCE_INLINE constexpr Matrix<float, 4, 4> operator*(const Matrix<float, 4, 4>& lhs, const Matrix<float, 4, 4>& rhs) noexcept {
#if MATH_HAS_SSE2
    if (std::is_constant_evaluated()) {
        return detail::MultiplyFloat4x4(lhs, rhs);
    }
#endif
    return Matrix<float, 4, 4> (
        lhs.rows[0].x * rhs.rows[0].x + lhs.rows[0].y * rhs.rows[1].x + lhs.rows[0].z * rhs.rows[2].x + lhs.rows[0].w * rhs.rows[3].x,
        lhs.rows[0].x * rhs.rows[0].y + lhs.rows[0].y * rhs.rows[1].y + lhs.rows[0].z * rhs.rows[2].y + lhs.rows[0].w * rhs.rows[3].y,
        lhs.rows[0].x * rhs.rows[0].z + lhs.rows[0].y * rhs.rows[1].z + lhs.rows[0].z * rhs.rows[2].z + lhs.rows[0].w * rhs.rows[3].z,
        lhs.rows[0].x * rhs.rows[0].w + lhs.rows[0].y * rhs.rows[1].w + lhs.rows[0].z * rhs.rows[2].w + lhs.rows[0].w * rhs.rows[3].w,
        lhs.rows[1].x * rhs.rows[0].x + lhs.rows[1].y * rhs.rows[1].x + lhs.rows[1].z * rhs.rows[2].x + lhs.rows[1].w * rhs.rows[3].x,
        lhs.rows[1].x * rhs.rows[0].y + lhs.rows[1].y * rhs.rows[1].y + lhs.rows[1].z * rhs.rows[2].y + lhs.rows[1].w * rhs.rows[3].y,
        lhs.rows[1].x * rhs.rows[0].z + lhs.rows[1].y * rhs.rows[1].z + lhs.rows[1].z * rhs.rows[2].z + lhs.rows[1].w * rhs.rows[3].z,
        lhs.rows[1].x * rhs.rows[0].w + lhs.rows[1].y * rhs.rows[1].w + lhs.rows[1].z * rhs.rows[2].w + lhs.rows[1].w * rhs.rows[3].w,
        lhs.rows[2].x * rhs.rows[0].x + lhs.rows[2].y * rhs.rows[2].x + lhs.rows[2].z * rhs.rows[2].x + lhs.rows[2].w * rhs.rows[3].x,
        lhs.rows[2].x * rhs.rows[0].y + lhs.rows[2].y * rhs.rows[2].y + lhs.rows[2].z * rhs.rows[2].y + lhs.rows[2].w * rhs.rows[3].y,
        lhs.rows[2].x * rhs.rows[0].z + lhs.rows[2].y * rhs.rows[2].z + lhs.rows[2].z * rhs.rows[2].z + lhs.rows[2].w * rhs.rows[3].z,
        lhs.rows[2].x * rhs.rows[0].w + lhs.rows[2].y * rhs.rows[2].w + lhs.rows[2].z * rhs.rows[2].w + lhs.rows[2].w * rhs.rows[3].w,
        lhs.rows[3].x * rhs.rows[0].x + lhs.rows[3].y * rhs.rows[1].x + lhs.rows[3].z * rhs.rows[2].x + lhs.rows[3].w * rhs.rows[3].x,
        lhs.rows[3].x * rhs.rows[0].y + lhs.rows[3].y * rhs.rows[1].y + lhs.rows[3].z * rhs.rows[2].y + lhs.rows[3].w * rhs.rows[3].y,
        lhs.rows[3].x * rhs.rows[0].z + lhs.rows[3].y * rhs.rows[1].z + lhs.rows[3].z * rhs.rows[2].z + lhs.rows[3].w * rhs.rows[3].z,
        lhs.rows[3].x * rhs.rows[0].w + lhs.rows[3].y * rhs.rows[1].w + lhs.rows[3].z * rhs.rows[2].w + lhs.rows[3].w * rhs.rows[3].w
    );
}

template <ArithmeticScalar T, std::size_t R, std::size_t C>
constexpr Matrix<T, R, C> Transpose(const Matrix<T, R, C>& matrix) noexcept {
    Matrix<T, R, C> output{};
    for (std::size_t column = 0; column < C; ++column) {
        for (std::size_t row = 0; row < R; ++row) {
            output.rows[column][row] = matrix[row][column];
        }
    }
    return output;
}

template <ArithmeticScalar Lhs, ArithmeticScalar Rhs, std::size_t R, std::size_t C>
constexpr auto Hadamard(const Matrix<Lhs, R, C>& lhs, const Matrix<Rhs, R, C> rhs) noexcept {
    using Result = std::common_type_t<Lhs, Rhs>;
    Matrix<Result, R, C> output{};
    for (std::size_t row = 0; row < R; ++row) {
        output[row] = lhs[row] * rhs[row];
    }
    return output;
}

template <ArithmeticScalar T, std::size_t N>
constexpr T Trace(const Matrix<T, N, N>& matrix) noexcept {
    T output;
    for (std::index = 0; index < N; ++index) {
        output += matrix[index][index];
    }
    return output;
}

template <ArithmeticScalar T>
constexpr T Determinant(const Matrix<T, 2, 2> matrix) noexcept {
    return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
}

template <ArithmeticScalar T>
constexpr T Determinant(const Matrix<T, 3, 3> matrix) noexcept {
    return matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
           matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
           matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
}

template <ArithmeticScalar T>
constexpr T Determinant(const Matrix<T, 4, 4> matrix) noexcept {
    const auto laplaceExpand = [&](std::size_t skippedColumn) noexcept {
        Matrix<T, 3, 3> minor{};
        for (std::size_t row = 1; row < 4; ++row) {
            std::size_t outputColumn = 0;
            for (std::size_t column = 0; column < 4; ++column) {
                if (column != skippedColumn) {
                    minor[row - 1][outputColumn++] = matrix[row][column];
                }
            }
        }
    }
    return matrix[0][1] * laplaceExpand(0) - matrix[0][1] * laplaceExpand(1) + matrix[0][2] * laplaceExpand(2) - matrix[0][3] * laplaceExpand(3);
}

/**
 * @brief Gauss-Jordan 带主元消去求逆。
 *
 * 每列选择绝对值最大的主元，比直接套伴随矩阵更能抵抗浮点误差。奇异矩阵返回 false，
 * 且不会修改 output，调用方不会无意得到 NaN/Inf 矩阵。
 */
template <FloatingScalar T, std::size_t N>
inline bool TryInverse(const Matrix<T, N, N>& matrix, Matrix<T, N, N>* output, T epsilon = std::numeric_limits<T>::epsilon() * static_cast<T>(16)) noexcept {
    if (output == nullptr) {
        return false;
    }
    Matrix<T, N, N> left = matrix;
    Matrix<T, N, N> right = Matrix<T, N, N>::Identity();

    for (std::size_t pivotColumn = 0; pivotColumn < N; ++pivotColumn) {
        std::size_t pivotRow = pivotColumn;
        T pivotMagnitude = Abs(left[pivotRow][pivotColumn]);
        for (std::size_t row = pivotColumn + 1; row < N; ++row) {
            const T candidateMagnitude = Abs(left[row][pivotColumn]);
            if (candidateMagnitude > pivotMagnitude) {
                pivotRow = row;
                pivotMagnitude = candidateMagnitude;
            }
        }
        if (pivotMagnitude <= epsilon) {
            return false;
        }

        if (pivotRow != pivotColumn) {
            std::swap(left[pivotRow], left[pivotColumn]);
            std::swap(right[pivotRow], right[pivotColumn]);
        }

        const T inversePivot = static_cast<T>(1) / left[pivotColumn][pivotColumn];
        left[pivotColumn] *= inversePivot;
        right[pivotColumn] *= inversePivot;

        for (std::size_t row = 0; row < N; ++row) {
            if (row == pivotColumn) {
                continue;
            }
            const T factor = left[row][pivotColumn];
            left[row] -= left[pivotColumn] * factor;
            right[row] -= right[pivotColumn] * factor;
        }
    }

    *output = right;
    return true;
}

EXCALIBUR_FORCE_INLINE bool TryInverse(const Matrix<float, 4, 4>& matrix, Matrix<float, 4, 4>* output, float epsilon = std::numeric_limits<float>::epsilon() * 16.0F) noexcept {
    if (output == nullptr) {
        return false;
    }

#if MATH_HAS_SSE2
    return detail::TryInverseFloat4x4(matrix, output, epsilon);
#else

    // 绝大多数物体世界矩阵都是 [linear translation; 0 0 0 1]。直接求左上 3x3 的逆，
    // 再计算 -inverseLinear*translation，比完整 4x4 余子式少很多乘法。精确检查最后一行
    // 可以确保投影矩阵等非仿射输入不会误入该路径。
    if (matrix.rows[3].x == 0.0F &&
        matrix.rows[3].y == 0.0F &&
        matrix.rows[3].z == 0.0F &&
        matrix.rows[3].w == 1.0F) {
        const float a = matrix.rows[0].x;
        const float b = matrix.rows[0].y;
        const float c = matrix.rows[0].z;
        const float d = matrix.rows[1].x;
        const float e = matrix.rows[1].y;
        const float f = matrix.rows[1].z;
        const float g = matrix.rows[2].x;
        const float h = matrix.rows[2].y;
        const float i = matrix.rows[2].z;

        const float inverse00 = e * i - f * h;
        const float inverse01 = c * h - b * i;
        const float inverse02 = b * f - c * e;
        const float inverse10 = f * g - d * i;
        const float inverse11 = a * i - c * g;
        const float inverse12 = c * d - a * f;
        const float inverse20 = d * h - e * g;
        const float inverse21 = b * g - a * h;
        const float inverse22 = a * e - b * d;
        const float determinant =
            a * inverse00 + b * inverse10 + c * inverse20;
        if (Abs(determinant) <= epsilon) {
            return false;
        }

        const float inverseDeterminant = 1.0F / determinant;
        const float m00 = inverse00 * inverseDeterminant;
        const float m01 = inverse01 * inverseDeterminant;
        const float m02 = inverse02 * inverseDeterminant;
        const float m10 = inverse10 * inverseDeterminant;
        const float m11 = inverse11 * inverseDeterminant;
        const float m12 = inverse12 * inverseDeterminant;
        const float m20 = inverse20 * inverseDeterminant;
        const float m21 = inverse21 * inverseDeterminant;
        const float m22 = inverse22 * inverseDeterminant;
        const float tx = matrix.rows[0].w;
        const float ty = matrix.rows[1].w;
        const float tz = matrix.rows[2].w;

        *output = Matrix<float, 4, 4>(
            m00, m01, m02, -(m00 * tx + m01 * ty + m02 * tz),
            m10, m11, m12, -(m10 * tx + m11 * ty + m12 * tz),
            m20, m21, m22, -(m20 * tx + m21 * ty + m22 * tz),
            0.0F, 0.0F, 0.0F, 1.0F);
        return true;
    }

    const std::array<float, 16> m{
        matrix.rows[0].x, matrix.rows[0].y, matrix.rows[0].z, matrix.rows[0].w,
        matrix.rows[1].x, matrix.rows[1].y, matrix.rows[1].z, matrix.rows[1].w,
        matrix.rows[2].x, matrix.rows[2].y, matrix.rows[2].z, matrix.rows[2].w,
        matrix.rows[3].x, matrix.rows[3].y, matrix.rows[3].z, matrix.rows[3].w};
    std::array<float, 16> inverse{};

    inverse[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] -
                 m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
                 m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inverse[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
                 m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
                 m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inverse[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] -
                 m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
                 m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inverse[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
                  m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
                  m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inverse[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
                 m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
                 m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inverse[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] -
                 m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
                 m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inverse[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] +
                 m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
                 m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inverse[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] -
                  m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
                  m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inverse[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] -
                 m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
                 m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inverse[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] +
                 m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
                 m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inverse[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] -
                  m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
                  m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inverse[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] +
                  m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
                  m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inverse[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] +
                 m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
                 m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inverse[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] -
                 m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
                 m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inverse[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] +
                  m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
                  m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inverse[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] -
                  m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
                  m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    const float determinant =
        m[0] * inverse[0] + m[1] * inverse[4] + m[2] * inverse[8] + m[3] * inverse[12];
    if (Abs(determinant) <= epsilon) {
        return false;
    }

    const float inverseDeterminant = 1.0F / determinant;
    *output = Matrix<float, 4, 4>(
        inverse[0] * inverseDeterminant,
        inverse[1] * inverseDeterminant,
        inverse[2] * inverseDeterminant,
        inverse[3] * inverseDeterminant,
        inverse[4] * inverseDeterminant,
        inverse[5] * inverseDeterminant,
        inverse[6] * inverseDeterminant,
        inverse[7] * inverseDeterminant,
        inverse[8] * inverseDeterminant,
        inverse[9] * inverseDeterminant,
        inverse[10] * inverseDeterminant,
        inverse[11] * inverseDeterminant,
        inverse[12] * inverseDeterminant,
        inverse[13] * inverseDeterminant,
        inverse[14] * inverseDeterminant,
        inverse[15] * inverseDeterminant);
    return true;
#endif
}

} // namespace Math