#pragma once

#include "../AliasDefinitions.hpp"

#include <bit>
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

namespace Math {

template <typename T>
concept Scalar = std::is_arithmetic_v<T>;

template <typename T>
concept ArithmeticScalar = Scalar<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <typename T>
concept FloatingScalar = std::floating_point<T>;

template <typename T>
concept IntergerScalar = std::integral<T> && !std::same_as<std::remove_cv_t<T>, bool>;

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T PI           = static_cast<T>(3.141592653589793238462643383279502884L);

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T TwoPI        = PI<T> * static_cast<T>(2);

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T HalfPI       = PI<T> * static_cast<T>(0.5);

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T QuarterPI    = PI<T> * static_cast<T>(0.25);

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T InvPI        = static_cast<T>(1) / PI<T>;

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T SqrtTwo      = static_cast<T>(1.414213562373095048801688724209698079L);

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE constexpr T GoldenRadio  = static_cast<T>(1.618033988749894848204586834365638118L);

template <typename T>
constexpr T Min(const T& lhs, const T& rhs) noexcept {
    return rhs < lhs ? rhs : lhs;
}

template <typename T>
constexpr T Max(const T& lhs, const T& rhs) noexcept {
    return lhs < rhs ? rhs : lhs;
}

template <typename T>
constexpr T Clamp(T value, T minimum, T maximum) noexcept {
    return value < minimum ? minimum : (maximum < value ? maximum : value);
}

template <ArithmeticScalar T>
constexpr T Saturate(T value) noexcept {
    return Clamp(value, static_cast<T>(0), static_cast<T>(1));
}

template <ArithmeticScalar T, ArithmeticScalar U>
constexpr auto Lerp(T start, T end, U amount) noexcept {
    using Result = std::common_type_t<T, U>; ///> <int, float> -> Relust = float, <float, double> -> Result = double
    const Result t = static_cast<Result>(amount);
    return static_cast<Result>(start) + (static_cast<Result>(end) - static_cast<Result>(start)) * t;
}

} // namespace Math