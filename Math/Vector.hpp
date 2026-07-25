#pragma once

#include "Scalar.hpp"

#include <array>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Math {

template <Scalar T, std::size_t N>
struct Vector;

namespace detail {

///> consteval 函数每一次调用都必须在编译期执行。
template <std::size_t... Indices>
consteval bool IndicesAreUnique() {
    constexpr std::array<std::size_t, sizeof...(Indices)> values{Indices...};
    for (std::size_t lhs = 0; lhs < values.size(); ++lhs) {
        for (std::size_t rhs = lhs + 1; rhs < values.size(); ++rhs) {
            if (values[lhs] == values[rhs]) {
                return false;
            }
        }
    }
    return true;
}

template <typename T>
using FloatingResult = std::conditional_t<std::floating_point<T>, T, double>;

} // namespace detail

#define DEFINE_SWIZZLE_2(A, AI, B, BI)                                                             \
    constexpr Vector<T, 2> A##B() const noexcept { return Swizzle<AI, BI>(); }
#define DEFINE_SWIZZLE_3(A, AI, B, BI, C, CI)                                                      \
    constexpr Vector<T, 3> A##B##C() const noexcept { return Swizzle<AI, BI, CI>(); }
#define DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, D, DI)                                               \
    constexpr Vector<T, 4> A##B##C##D() const noexcept { return Swizzle<AI, BI, CI, DI>(); }

#define SWIZZLE_2_ROW_2(A, AI, C0, I0, C1, I1)                                                     \
    DEFINE_SWIZZLE_2(A, AI, C0, I0)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C1, I1)
#define SWIZZLE_3_PAIR_2(A, AI, B, BI, C0, I0, C1, I1)                                             \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C0, I0)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C1, I1)
#define SWIZZLE_3_ROW_2(A, AI, C0, I0, C1, I1)                                                     \
    SWIZZLE_3_PAIR_2(A, AI, C0, I0, C0, I0, C1, I1)                                                \
    SWIZZLE_3_PAIR_2(A, AI, C1, I1, C0, I0, C1, I1)
#define SWIZZLE_4_TRIPLE_2(A, AI, B, BI, C, CI, C0, I0, C1, I1)                                    \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C0, I0)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C1, I1)
#define SWIZZLE_4_PAIR_2(A, AI, B, BI, C0, I0, C1, I1)                                             \
    SWIZZLE_4_TRIPLE_2(A, AI, B, BI, C0, I0, C0, I0, C1, I1)                                       \
    SWIZZLE_4_TRIPLE_2(A, AI, B, BI, C1, I1, C0, I0, C1, I1)
#define SWIZZLE_4_ROW_2(A, AI, C0, I0, C1, I1)                                                     \
    SWIZZLE_4_PAIR_2(A, AI, C0, I0, C0, I0, C1, I1)                                                \
    SWIZZLE_4_PAIR_2(A, AI, C1, I1, C0, I0, C1, I1)
#define DEFINE_SWIZZLES_2_COMPONENTS(C0, I0, C1, I1)                                               \
    SWIZZLE_2_ROW_2(C0, I0, C0, I0, C1, I1)                                                        \
    SWIZZLE_2_ROW_2(C1, I1, C0, I0, C1, I1)                                                        \
    SWIZZLE_3_ROW_2(C0, I0, C0, I0, C1, I1)                                                        \
    SWIZZLE_3_ROW_2(C1, I1, C0, I0, C1, I1)                                                        \
    SWIZZLE_4_ROW_2(C0, I0, C0, I0, C1, I1)                                                        \
    SWIZZLE_4_ROW_2(C1, I1, C0, I0, C1, I1)

#define SWIZZLE_2_ROW_3(A, AI, C0, I0, C1, I1, C2, I2)                                             \
    DEFINE_SWIZZLE_2(A, AI, C0, I0)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C1, I1)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C2, I2)
#define SWIZZLE_3_PAIR_3(A, AI, B, BI, C0, I0, C1, I1, C2, I2)                                     \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C0, I0)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C1, I1)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C2, I2)
#define SWIZZLE_3_ROW_3(A, AI, C0, I0, C1, I1, C2, I2)                                             \
    SWIZZLE_3_PAIR_3(A, AI, C0, I0, C0, I0, C1, I1, C2, I2)                                        \
    SWIZZLE_3_PAIR_3(A, AI, C1, I1, C0, I0, C1, I1, C2, I2)                                        \
    SWIZZLE_3_PAIR_3(A, AI, C2, I2, C0, I0, C1, I1, C2, I2)
#define SWIZZLE_4_TRIPLE_3(A, AI, B, BI, C, CI, C0, I0, C1, I1, C2, I2)                            \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C0, I0)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C1, I1)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C2, I2)
#define SWIZZLE_4_PAIR_3(A, AI, B, BI, C0, I0, C1, I1, C2, I2)                                     \
    SWIZZLE_4_TRIPLE_3(A, AI, B, BI, C0, I0, C0, I0, C1, I1, C2, I2)                               \
    SWIZZLE_4_TRIPLE_3(A, AI, B, BI, C1, I1, C0, I0, C1, I1, C2, I2)                               \
    SWIZZLE_4_TRIPLE_3(A, AI, B, BI, C2, I2, C0, I0, C1, I1, C2, I2)
#define SWIZZLE_4_ROW_3(A, AI, C0, I0, C1, I1, C2, I2)                                             \
    SWIZZLE_4_PAIR_3(A, AI, C0, I0, C0, I0, C1, I1, C2, I2)                                        \
    SWIZZLE_4_PAIR_3(A, AI, C1, I1, C0, I0, C1, I1, C2, I2)                                        \
    SWIZZLE_4_PAIR_3(A, AI, C2, I2, C0, I0, C1, I1, C2, I2)
#define DEFINE_SWIZZLES_3_COMPONENTS(C0, I0, C1, I1, C2, I2)                                       \
    SWIZZLE_2_ROW_3(C0, I0, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_2_ROW_3(C1, I1, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_2_ROW_3(C2, I2, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_3_ROW_3(C0, I0, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_3_ROW_3(C1, I1, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_3_ROW_3(C2, I2, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_4_ROW_3(C0, I0, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_4_ROW_3(C1, I1, C0, I0, C1, I1, C2, I2)                                                \
    SWIZZLE_4_ROW_3(C2, I2, C0, I0, C1, I1, C2, I2)

#define SWIZZLE_2_ROW_4(A, AI, C0, I0, C1, I1, C2, I2, C3, I3)                                     \
    DEFINE_SWIZZLE_2(A, AI, C0, I0)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C1, I1)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C2, I2)                                                                \
    DEFINE_SWIZZLE_2(A, AI, C3, I3)
#define SWIZZLE_3_PAIR_4(A, AI, B, BI, C0, I0, C1, I1, C2, I2, C3, I3)                             \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C0, I0)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C1, I1)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C2, I2)                                                         \
    DEFINE_SWIZZLE_3(A, AI, B, BI, C3, I3)
#define SWIZZLE_3_ROW_4(A, AI, C0, I0, C1, I1, C2, I2, C3, I3)                                     \
    SWIZZLE_3_PAIR_4(A, AI, C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_3_PAIR_4(A, AI, C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_3_PAIR_4(A, AI, C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_3_PAIR_4(A, AI, C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)
#define SWIZZLE_4_TRIPLE_4(A, AI, B, BI, C, CI, C0, I0, C1, I1, C2, I2, C3, I3)                    \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C0, I0)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C1, I1)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C2, I2)                                                  \
    DEFINE_SWIZZLE_4(A, AI, B, BI, C, CI, C3, I3)
#define SWIZZLE_4_PAIR_4(A, AI, B, BI, C0, I0, C1, I1, C2, I2, C3, I3)                             \
    SWIZZLE_4_TRIPLE_4(A, AI, B, BI, C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                       \
    SWIZZLE_4_TRIPLE_4(A, AI, B, BI, C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                       \
    SWIZZLE_4_TRIPLE_4(A, AI, B, BI, C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                       \
    SWIZZLE_4_TRIPLE_4(A, AI, B, BI, C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)
#define SWIZZLE_4_ROW_4(A, AI, C0, I0, C1, I1, C2, I2, C3, I3)                                     \
    SWIZZLE_4_PAIR_4(A, AI, C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_4_PAIR_4(A, AI, C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_4_PAIR_4(A, AI, C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                                \
    SWIZZLE_4_PAIR_4(A, AI, C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)
#define DEFINE_SWIZZLES_4_COMPONENTS(C0, I0, C1, I1, C2, I2, C3, I3)                               \
    SWIZZLE_2_ROW_4(C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_2_ROW_4(C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_2_ROW_4(C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_2_ROW_4(C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_3_ROW_4(C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_3_ROW_4(C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_3_ROW_4(C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_3_ROW_4(C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_4_ROW_4(C0, I0, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_4_ROW_4(C1, I1, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_4_ROW_4(C2, I2, C0, I0, C1, I1, C2, I2, C3, I3)                                        \
    SWIZZLE_4_ROW_4(C3, I3, C0, I0, C1, I1, C2, I2, C3, I3)
    
template <Scalar T>
struct Vector<T, 2> {
    using ValueType = T;
    static constexpr std::size_t ComponentCount = 2;

    T x{}, y{};

    constexpr Vector() = default;
    explicit constexpr Vector(T value) noexcept : x(value), y(value) {}
    constexpr Vector(T xValue, T yValue) noexcept : x(xValue), y(yValue) {};
    template <Scalar U>
    explicit constexpr Vector(const Vector<U, 2>& other) noexcept : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

    constexpr T& operator[](std::size_t index) noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : y;
    }

    constexpr const T& operator[](std::size_t index) const noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : y;
    }

    constexpr T r() const noexcept { return x; }
    constexpr T g() const noexcept { return y; }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) >= 2 && sizeof...(Indices) <= 4 && ((Indices < ComponentCount) && ...))
    constexpr Vector<T, sizeof...(Indices)> Swizzle() const noexcept {
        return Vector<T, sizeof...(Indices)>((*this)[Indices]...);
    }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) == 2 && ((Indices < ComponentCount) && ...) && detail::IndicesAreUnique<Indices...>())
    constexpr void SetSwizzle(const Vector<T, sizeof...(Indices)>& value) noexcept {
        std::size_t source = 0;
        (((*this)[Indices] = value[source++]), ...);
    }

    DEFINE_SWIZZLES_2_COMPONENTS(x, 0, y, 1)
    DEFINE_SWIZZLES_2_COMPONENTS(r, 0, g, 1)
};

template <Scalar T>
struct Vector<T, 3> {
    using ValueType = T;
    static constexpr std::size_t ComponentCount = 3;

    T x{}, y{}, z{};

    constexpr Vector() = default;
    explicit constexpr Vector(T value) noexcept : x(value), y(value), z(value) {}
    constexpr Vector(T xValue, T yValue, T zValue) noexcept : x(xValue), y(yValue), z(zValue) {};
    constexpr Vector(const Vector<T, 2>& xyValue, T zValue) noexcept : x(xyValue.x), y(xyValue.y), z(zValue) {}
    template <Scalar U>
    explicit constexpr Vector(const Vector<U, 3>& other) noexcept : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}
    template <Scalar U>
    explicit constexpr Vector(const Vector<U, 4>& other) noexcept : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}

    constexpr T& operator[](std::size_t index) noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : (index == 1 ? y : z);
    }

    constexpr const T& operator[](std::size_t index) const noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : (index == 1 ? y : z);
    }

    constexpr T r() const noexcept { return x; }
    constexpr T g() const noexcept { return y; }
    constexpr T b() const noexcept { return z; }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) >= 2 && sizeof...(Indices) <= 4 && ((Indices < ComponentCount) && ...))
    constexpr Vector<T, sizeof...(Indices)> Swizzle() const noexcept {
        return Vector<T, sizeof...(Indices)>((*this)[Indices]...);
    }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) >= 2 && sizeof...(Indices) <= 3 && ((Indices < ComponentCount) && ...) && detail::IndicesAreUnique<Indices...>())
    constexpr void SetSwizzle(const Vector<T, sizeof...(Indices)>& value) noexcept {
        std::size_t source = 0;
        (((*this)[Indices] = value[source++]), ...);
    }

    DEFINE_SWIZZLES_3_COMPONENTS(x, 0, y, 1, z, 2)
    DEFINE_SWIZZLES_3_COMPONENTS(r, 0, g, 1, b, 2)
};

template <Scalar T>
struct Vector<T, 4> {
    using ValueType = T;
    static constexpr std::size_t ComponentCount = 4;

    T x{}, y{}, z{}, w{};

    constexpr Vector() = default;
    explicit constexpr Vector(T value) noexcept : x(value), y(value), z(value), w(value) {}
    constexpr Vector(T xValue, T yValue, T zValue, T wValue) noexcept : x(xValue), y(yValue), z(zValue), w(wValue) {};
    constexpr Vector(const Vector<T, 2>& xyValue, const Vector<T, 2>& zwValue) noexcept : x(xyValue.x), y(xyValue.y), z(zwValue.x), w(zwValue.y) {}
    constexpr Vector(const Vector<T, 3>& xyzValue, T wValue) noexcept : x(xyzValue.x), y(xyzValue.y), z(xyzValue.z), w(wValue) {}
    template <Scalar U>
    explicit constexpr Vector(const Vector<U, 4>& other) noexcept : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)), w(static_cast<T>(other.w)) {}

    constexpr T& operator[](std::size_t index) noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }

    constexpr const T& operator[](std::size_t index) const noexcept {
        assert(index < ComponentCount);
        return index == 0 ? x : (index == 1 ? y : (index == 2 ? z : w));
    }

    constexpr T r() const noexcept { return x; }
    constexpr T g() const noexcept { return y; }
    constexpr T b() const noexcept { return z; }
    constexpr T a() const noexcept { return w; }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) >= 2 && sizeof...(Indices) <= 4 && ((Indices < ComponentCount) && ...))
    constexpr Vector<T, sizeof...(Indices)> Swizzle() const noexcept {
        return Vector<T, sizeof...(Indices)>((*this)[Indices]...);
    }

    template <std::size_t... Indices>
    requires(sizeof...(Indices) >= 2 && sizeof...(Indices) <= 4 && ((Indices < ComponentCount) && ...) && detail::IndicesAreUnique<Indices...>())
    constexpr void SetSwizzle(const Vector<T, sizeof...(Indices)>& value) noexcept {
        std::size_t source = 0;
        (((*this)[Indices] = value[source++]), ...);
    }

    DEFINE_SWIZZLES_4_COMPONENTS(x, 0, y, 1, z, 2, w, 3)
    DEFINE_SWIZZLES_4_COMPONENTS(r, 0, g, 1, b, 2, a, 3)
};

#undef DEFINE_SWIZZLES_4_COMPONENTS
#undef SWIZZLE_4_ROW_4
#undef SWIZZLE_4_PAIR_4
#undef SWIZZLE_4_TRIPLE_4
#undef SWIZZLE_3_ROW_4
#undef SWIZZLE_3_PAIR_4
#undef SWIZZLE_2_ROW_4
#undef DEFINE_SWIZZLES_3_COMPONENTS
#undef SWIZZLE_4_ROW_3
#undef SWIZZLE_4_PAIR_3
#undef SWIZZLE_4_TRIPLE_3
#undef SWIZZLE_3_ROW_3
#undef SWIZZLE_3_PAIR_3
#undef SWIZZLE_2_ROW_3
#undef DEFINE_SWIZZLES_2_COMPONENTS
#undef SWIZZLE_4_ROW_2
#undef SWIZZLE_4_PAIR_2
#undef SWIZZLE_4_TRIPLE_2
#undef SWIZZLE_3_ROW_2
#undef SWIZZLE_3_PAIR_2
#undef SWIZZLE_2_ROW_2
#undef DEFINE_SWIZZLE_4
#undef DEFINE_SWIZZLE_3
#undef DEFINE_SWIZZLE_2

template <Scalar T, std::size_t N>
constexpr bool operator==(const Vector<T, N>& lhs, const Vector<T, N>& rhs) {
    for (std::size_t index = 0; index < N; ++index) {
        if (lhs[index] != rhs[index]) {
            return false;
        }
    }
    return true;
}

template <Scalar T, std::size_t N>
constexpr bool operator!=(const Vector<T, N>&lhs, const Vector<T, N>& rhs) {
    return !(lhs == rhs);
}

#define DEFINE_VECTOR_BINARY_OPERATOR(OPERATOR)                                                        \
    template <ArithmeticScalar L, ArithmeticScalar R, std::size_t N>                                   \
    constexpr auto operator OPERATOR(const Vector<L, N>& lhs, const Vector<R, N>& rhs) noexcept {      \
        using Result = std::common_type_t<L, R>;                                                       \
        Vector<Result, N> output{};                                                                    \
        for (std::size_t index = 0; index < N; ++index) {                                              \
            output[index] = static_cast<Result>(lhs[index]) OPERATOR static_cast<Result>(rhs[index]);  \
        }                                                                                              \
        return output;                                                                                 \
    }                                                                                                  \
    template <ArithmeticScalar L, ArithmeticScalar R, std::size_t N>                                   \
    constexpr auto operator OPERATOR(const Vector<L, N>& lhs, R rhs) {                                 \
        return lhs OPERATOR Vector<R, N>(rhs);                                                         \
    }                                                                                                  \
    template <ArithmeticScalar L, ArithmeticScalar R, std::size_t N>                                   \
    constexpr auto operator OPERATOR(L lhs, const Vector<R, N>& rhs) {                                 \
        return Vector<L, N>(lhs) OPERATOR rhs;                                                         \
    }

DEFINE_VECTOR_BINARY_OPERATOR(+)
DEFINE_VECTOR_BINARY_OPERATOR(-)
DEFINE_VECTOR_BINARY_OPERATOR(*)
DEFINE_VECTOR_BINARY_OPERATOR(/)

#undef DEFINE_VECTOR_BINARY_OPERATOR

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> operator-(const Vector<T, N>& value) {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = -value[index];
    }
    return output;
}

#define DEFINE_VECTOR_COMPOUND_OPERATOR(OPERATOR)                                                     \
    template <ArithmeticScalar T, ArithmeticScalar U, std::size_t N>                                  \
    constexpr Vector<T, N>& operator OPERATOR(Vector<T, N>& lhs, const Vector<U, N>& rhs) noexcept {  \
        for (std::size_t index = 0; index < N; ++index) {                                             \
            lhs[index] OPERATOR static_cast<T>(rhs[index]);                                           \
        }                                                                                             \
        return lhs;                                                                                   \
    }                                                                                                 \
    template <ArithmeticScalar T, ArithmeticScalar U, std::size_t N>                                  \
    constexpr Vector<T, N>& operator OPERATOR(Vector<T, N>& lhs, U rhs) {                             \
        return lhs OPERATOR Vector<U, N>(rhs);                                                        \
    }

DEFINE_VECTOR_COMPOUND_OPERATOR(+=)
DEFINE_VECTOR_COMPOUND_OPERATOR(-=)
DEFINE_VECTOR_COMPOUND_OPERATOR(*=)
DEFINE_VECTOR_COMPOUND_OPERATOR(/=)

#undef DEFINE_VECTOR_COMPOUND_OPERATOR

#define DEFINE_VECTOR_INTEGRAL_OPERATOR(OPERATOR)                                                     \
    template <IntergerScalar L, IntergerScalar R, std::size_t N>                                      \
    constexpr auto operator OPERATOR(const Vector<L, N>& lhs, const Vector<R, N>& rhs) noexcept {     \
        using Result = std::common_type_t<L, R>;                                                      \
        Vector<Result, N> output{};                                                                   \
        for (std::size_t index = 0; index < N; ++index) {                                             \
            output[index] = static_cast<Result>(lhs[index]) OPERATOR static_cast<Result>(rhs[index]); \
        }                                                                                             \
        return output;                                                                                \
    }

DEFINE_VECTOR_INTEGRAL_OPERATOR(%)
DEFINE_VECTOR_INTEGRAL_OPERATOR(&)
DEFINE_VECTOR_INTEGRAL_OPERATOR(|)
DEFINE_VECTOR_INTEGRAL_OPERATOR(^)

#undef DEFINE_VECTOR_INTEGRAL_OPERATOR

template <IntergerScalar T, std::size_t N>
constexpr Vector<T, N> operator~(const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = static_cast<T>(~value[index]);
    }
    return output;
}

template <std::size_t N>
constexpr Vector<bool, N> operator~(const Vector<bool, N>& value) noexcept {
    Vector<bool, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = !value[index];
    }
    return output;
}

template <std::size_t N>
constexpr bool Any(const Vector<bool, N>& value) noexcept {
    for (std::size_t index = 0; index < N; ++index) {
        if (value[index]) {
            return true;
        }
    }
    return false;
}

template <std::size_t N>
constexpr bool All(const Vector<bool, N>& value) noexcept {
    for (std::size_t index = 0; index < N; ++index) {
        if (!value[index]) {
            return false;
        }
    }
    return true;
}

template <std::size_t N>
constexpr bool None(const Vector<bool, N>& value) noexcept {
    return !Any(value);
}

#define DEFINE_VECTOR_COMPARISON(NAME, OPERATOR)                                                    \
    template <Scalar L, Scalar R, std::size_t N>                                                    \
    constexpr Vector<bool, N> NAME(const Vector<L, N>& lhs, const Vector<R, N>& rhs) noexcept {     \
        Vector<bool, N> output{};                                                                   \
        for (std::size_t index = 0; index < N; ++index) {                                           \
            output[index] = lhs[index] OPERATOR rhs[index];                                         \
        }                                                                                           \
        return output;                                                                              \
    }

DEFINE_VECTOR_COMPARISON(Equal, ==)
DEFINE_VECTOR_COMPARISON(NotEqual, !=)
DEFINE_VECTOR_COMPARISON(Less, <)
DEFINE_VECTOR_COMPARISON(LessEqual, <=)
DEFINE_VECTOR_COMPARISON(Greater, >)
DEFINE_VECTOR_COMPARISON(GreaterEqual, >=)

#undef DEFINE_VECTOR_COMPARISON

template <Scalar To, Scalar From, std::size_t N>
constexpr Vector<To, N> VectorCast(const Vector<From, N>& value) noexcept {
    return Vector<To, N>(value);
}

template <Scalar T, std::size_t N>
constexpr std::array<T, N> ToArray(const Vector<T, N>& value) noexcept {
    std::array<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = value[index];
    }
    return output;
}

template <Scalar T, std::size_t N>
constexpr Vector<T, N> FromArray(const std::array<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = value[index];
    }
    return output;
}

template <ArithmeticScalar L, ArithmeticScalar R, std::size_t N>
constexpr auto Dot(const Vector<L, N>& lhs, const Vector<R, N>& rhs) noexcept {
    using Result = std::common_type_t<L, R>;
    Result output{};
    for (std::size_t index = 0; index < N; ++index) {
        output += static_cast<Result>(lhs[index]) * static_cast<Result>(rhs[index]);
    }
    return output;
}

EXCALIBUR_FORCE_INLINE constexpr float Dot(const Vector<float, 2>& lhs, const Vector<float, 2>& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

EXCALIBUR_FORCE_INLINE constexpr float Dot(const Vector<float, 3>& lhs, const Vector<float, 3>& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

EXCALIBUR_FORCE_INLINE constexpr float Dot(const Vector<float, 4>& lhs, const Vector<float, 4>& rhs) noexcept {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

template <ArithmeticScalar L, ArithmeticScalar R>
constexpr auto Cross(const Vector<L, 3>& lhs, const Vector<R, 3>& rhs) noexcept {
    using Result = std::common_type_t<L, R>;
    return Vector<Result, 3>(
        static_cast<Result>(lhs.y) * static_cast<Result>(rhs.z) - static_cast<Result>(lhs.z) * static_cast<Result>(rhs.y),
        static_cast<Result>(lhs.z) * static_cast<Result>(rhs.x) - static_cast<Result>(lhs.x) * static_cast<Result>(rhs.z),
        static_cast<Result>(lhs.x) * static_cast<Result>(rhs.y) - static_cast<Result>(lhs.y) * static_cast<Result>(rhs.x)
    );
}

EXCALIBUR_FORCE_INLINE constexpr Vector<float, 3> Cross(const Vector<float, 3>& lhs, const Vector<float, 3>& rhs) noexcept {
    return { lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x };
}

template <ArithmeticScalar T, std::size_t N>
inline auto LengthSquared(const Vector<T, N>& value) noexcept {
    return Dot(value, value);
}

template <ArithmeticScalar T, std::size_t N>
inline detail::FloatingResult<T> Length(const Vector<T, N>& value) noexcept {
    using Result = detail::FloatingResult<T>;
    return std::sqrt(static_cast<Result>(LengthSquared(value)));
}

EXCALIBUR_FORCE_INLINE constexpr float Length(const Vector<float, 3>& value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

template <ArithmeticScalar T, std::size_t N>
inline detail::FloatingResult<T> Distance(const Vector<T, N>& lhs, const Vector<T, N>& rhs) noexcept {
    return Length(lhs - rhs);
}

template <ArithmeticScalar T, std::size_t N>
inline Vector<detail::FloatingResult<T>, N> Normalize(const Vector<T, N>& value) noexcept {
    using Result = detail::FloatingResult<T>;
    const Result length = Length(value);
    if (length <= std::numeric_limits<Result>::epsilon()) {
        return Vector<Result, N>(static_cast<Result>(0));
    }
    return Vector<Result, N>(value) / length;
}

inline Vector<float, 3> Normalize(const Vector<float, 3>& value) noexcept {
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSquared <= std::numeric_limits<float>::epsilon() * std::numeric_limits<float>::epsilon()) {
        return Vector<float, 3>(0.0F);
    }
    const float inversedLength = 1.0F / std::sqrt(lengthSquared);
    return { value.x * inversedLength, value.y * inversedLength, value.z * inversedLength };
}

template <FloatingScalar T, std::size_t N>
inline Vector<T, N> NormalizeSafe(const Vector<T, N>& value, const Vector<T, N>& fallback, T epsilon = std::numeric_limits<T>::epsilon() * static_cast<T>(8)) noexcept {
    const T lengthSquared = LengthSquared(value);
    return lengthSquared <= epsilon * epsilon ? fallback : value / std::sqrt(lengthSquared);
}

inline Vector<float, 3> NormalizeSafe(const Vector<float, 3>& value, const Vector<float, 3>& fallback, float epsilon = std::numeric_limits<float>::epsilon() * static_cast<float>(8)) noexcept {
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    return lengthSquared <= epsilon * epsilon ? fallback : value / std::sqrt(lengthSquared);
}

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> Abs(const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = Abs(value[index]);
    }
    return output;
}

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> Min(const Vector<T, N>& lhs, const Vector<T, N>& rhs) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = Min(lhs[index], rhs[index]);
    }
    return output;
}

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> Max(const Vector<T, N>& lhs, const Vector<T, N>& rhs) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = Max(lhs[index], rhs[index]);
    }
    return output;
}

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> Clamp(const Vector<T, N>& value, const Vector<T, N>& minimum, const Vector<T, N>& maximum) noexcept {
    return Min(Max(value, minimum), maximum);
}

template <ArithmeticScalar T, std::size_t N>
constexpr Vector<T, N> Saturate(const Vector<T, N>& value) noexcept {
    return Min(Max(value, Vector<T, N>(static_cast<T>(0))), Vector<T, N>(static_cast<T>(1)));
}

template <ArithmeticScalar T, ArithmeticScalar U, std::size_t N>
constexpr auto Lerp(const Vector<T, N>& start, const Vector<T, N>& end, U amount) noexcept {
    return start + (end - start) * amount;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Floor(const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = std::floor(value[index]);
    }
    return output;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Ceil(const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = std::ceil(value[index]);
    }
    return output;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Round(const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = std::round(value[index]);
    }
    return output;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Fract(const Vector<T, N>& value) noexcept {
    return value - Floor(value);
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> SmoothStep(const Vector<T, N>& edge0, const Vector<T, N>& edge1, const Vector<T, N>& value) noexcept {
    Vector<T, N> output{};
    for (std::size_t index = 0; index < N; ++index) {
        output[index] = SmoothStep(edge0[index], edge1[index], value[index]);
    }
    return output;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Reflect(const Vector<T, N>& incident, const Vector<T, N>& normal) noexcept {
    return incident - static_cast<T>(2) * Dot(incident, normal) * normal;
}

template <FloatingScalar T, std::size_t N>
inline Vector<T, N> Refract(const Vector<T, N>& incident, const Vector<T, N>& normal, T eta) noexcept {
    // Snell 定律的向量形式，eta=n1/n2。判别式小于 0 表示全反射，此时返回零向量。
    const T normalDotIncident = Dot(normal, incident);
    const T discriminant = static_cast<T>(1) - eta * eta * (static_cast<T>(1) - normalDotIncident * normalDotIncident);
    if (discriminant < static_cast<T>(0)) {
        return Vector<T, N>(static_cast<T>(0));
    }
    return eta * incident - (eta * normalDotIncident + std::sqrt(discriminant)) * normal;
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Project(const Vector<T, N>& value, const Vector<T, N>& onto) noexcept {
    const T denominator = Dot(onto, onto);
    return denominator == static_cast<T>(0) ? Vector<T, N>(static_cast<T>(0)) : onto * (Dot(value, onto) / denominator);
}

template <FloatingScalar T, std::size_t N>
constexpr Vector<T, N> Reject(const Vector<T, N>& value, const Vector<T, N>& from) {
    return value - Project(value, from);
}

template <FloatingScalar T, std::size_t N>
inline T AngleBetween(const Vector<T, N>& lhs, const Vector<T, N>& rhs) {
    const T denominator = Length(lhs) * Length(rhs);
    if (denominator <= std::numeric_limits<T>::epsilon()) {
        return static_cast<T>(0);
    }
    return std::acos(Clamp(Dot(lhs, rhs) / denominator, static_cast<T>(-1), static_cast<T>(1)));
}

} // namespace Math

using bool2   =  Math::Vector<bool, 2>;
using bool3   =  Math::Vector<bool, 3>;
using bool4   =  Math::Vector<bool, 4>;
using byte2   =  Math::Vector<std::int8_t, 2>;
using byte3   =  Math::Vector<std::int8_t, 3>;
using byte4   =  Math::Vector<std::int8_t, 4>;
using sbyte2  =  Math::Vector<std::uint8_t, 2>;
using sbyte3  =  Math::Vector<std::uint8_t, 3>;
using sbyte4  =  Math::Vector<std::uint8_t, 4>;
using short2  =  Math::Vector<std::int16_t, 2>;
using short3  =  Math::Vector<std::int16_t, 3>;
using short4  =  Math::Vector<std::int16_t, 4>;
using ushort2 =  Math::Vector<std::uint16_t, 2>;
using ushort3 =  Math::Vector<std::uint16_t, 3>;
using ushort4 =  Math::Vector<std::uint16_t, 4>;
using int2    =  Math::Vector<std::int32_t, 2>;
using int3    =  Math::Vector<std::int32_t, 3>;
using int4    =  Math::Vector<std::int32_t, 4>;
using uint2   =  Math::Vector<std::uint32_t, 2>;
using uint3   =  Math::Vector<std::uint32_t, 3>;
using uint4   =  Math::Vector<std::uint32_t, 4>;
using long2   =  Math::Vector<std::int64_t, 2>;
using long3   =  Math::Vector<std::int64_t, 3>;
using long4   =  Math::Vector<std::int64_t, 4>;
using ulong2  =  Math::Vector<std::uint64_t, 2>;
using ulong3  =  Math::Vector<std::uint64_t, 3>;
using ulong4  =  Math::Vector<std::uint64_t, 4>;
using float2  =  Math::Vector<float, 2>;
using float3  =  Math::Vector<float, 3>;
using float4  =  Math::Vector<float, 4>;
using double2 =  Math::Vector<double, 2>;
using double3 =  Math::Vector<double, 3>;
using double4 =  Math::Vector<double, 4>;

using Math::Abs;
using Math::All;
using Math::AngleBetween;
using Math::Any;
using Math::Ceil;
using Math::Clamp;
using Math::Cross;
using Math::Distance;
using Math::Dot;
using Math::Equal;
using Math::Floor;
using Math::Fract;
using Math::FromArray;
using Math::Greater;
using Math::GreaterEqual;
using Math::Length;
using Math::LengthSquared;
using Math::Lerp;
using Math::Less;
using Math::LessEqual;
using Math::Max;
using Math::Min;
using Math::None;
using Math::Normalize;
using Math::NormalizeSafe;
using Math::NotEqual;
using Math::Project;
using Math::Reflect;
using Math::Refract;
using Math::Reject;
using Math::Round;
using Math::Saturate;
using Math::SmoothStep;
using Math::ToArray;
using Math::VectorCast;