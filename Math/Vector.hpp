#pragma once

#include "Scalar.hpp"

#include <array>
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
    constexpr Vector<T, 4> A##B##C##D() const noexcept {                                           \
        return Swizzle<AI, BI, CI, DI>();                                                          \
    }

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

    DEFINE_SWIZZLES_3_COMPONENTS(x, 0, y, 1)
    DEFINE_SWIZZLES_3_COMPONENTS(r, 0, g, 1)
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

    DEFINE_SWIZZLES_2_COMPONENTS(x, 0, y, 1, z, 2)
    DEFINE_SWIZZLES_2_COMPONENTS(r, 0, g, 1, b, 2)
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

} // namespace Math