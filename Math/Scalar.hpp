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

template <ArithmeticScalar T>
constexpr T Lerp(T start, T end, T t) noexcept {
    return start + (end - start) * t;
}

template <ArithmeticScalar T>
constexpr T LerpClamped(T start, T end, T t) noexcept {
    return start + (end - start) * Saturate(t);
}

/// 从线性插值公式反推出插值参数 t
template <FloatingScalar T>
constexpr T InverseLerp(T start, T end, T value) noexcept {
    return start == end ? static_cast<T>(0) : (value - start) / (end - start);
}

template <FloatingScalar T>
constexpr T Remap(T inputMinimum, T inputMaximum, T outputMinimum, T outputMaximum, T value) noexcept {
    return Lerp(outputMinimum, outputMaximum, InverseLerp(inputMinimum, inputMaximum, value));
}

template <ArithmeticScalar T>
constexpr T Square(T value) noexcept {
    return value * value;
}

template <ArithmeticScalar T>
constexpr T Cube(T value) noexcept {
    return value * value * value;
}

template <ArithmeticScalar T>
constexpr T Abs(T value) noexcept {
    if constexpr (std::unsigned_integral<T>) {
        return value;
    }
    else {
        return value < static_cast<T>(0) ? -value : value;
    }
}

template <ArithmeticScalar T>
constexpr int Sign(T value) noexcept {
    ///> true = 1, false = 0
    return (static_cast<T>(0) < value) - (value < static_cast<T>(0));
}

template <FloatingScalar T>
constexpr T Degrees(T radians) noexcept {
    return radians * (static_cast<T>(180) / PI<T>);
}

template <FloatingScalar T>
constexpr T Radians(T degrees) noexcept {
    return degrees * (PI<T> / static_cast<T>(180));
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE T Fract(T value) noexcept {
    return value - std::floor(value);
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE T Mod(T value, T divisor) noexcept {
    return std::fmod(value, divisor);
}

template <IntergerScalar T>
EXCALIBUR_FORCE_INLINE T Mod(T value, T divisor) noexcept {
    return value % divisor;
}

/// 把任意实数循环映射到区间 [0, length), 适合 UV、角度和循环动画。
template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE T Repeat(T value, T length) noexcept {
    if (length <= static_cast<T>(0)) {
        return static_cast<T>(0);
    }
    else {
        return value - std::floor(value / length) * length;
    }
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE T Wrap(T value, T minimum, T maximum) noexcept {
    const T range = maximum - minimum;
    return range <= static_cast<T>(0) ? minimum : minimum + Repeat(value - minimum, range);
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE T PingPong(T value, T length) noexcept {
    const T repeated = Repeat(value, length * static_cast<T>(2));
    return length - Abs(repeated - length);
}

template <ArithmeticScalar T>
constexpr T Step(T edge, T value) noexcept {
    return value < edge ? static_cast<T>(0) : static_cast<T>(1);
}

template <ArithmeticScalar T>
constexpr T SmoothStep(T edge0, T edge1, T value) noexcept {
    // 三次 Hermite 曲线 3t^2-2t^3：两端一阶导数为 0，过渡不会突然改变速度。
    const T t = Saturate(InverseLerp(edge0, edge1, value));
    return t * t * (static_cast<T>(3) - t * static_cast<T>(2));
}

template <ArithmeticScalar T>
constexpr T SmootherStep(T edge0, T edge1, T value) noexcept {
    const T t = Saturate(InverseLerp(edge0, edge1, value));
    return t * t * t * (t * (t * static_cast<T>(6) - static_cast<T>(15) + static_cast<T>(10)));
}

template <ArithmeticScalar T>
constexpr bool NearlyEqual(
    T lhs, 
    T rhs, 
    T absoluteEpsilon = std::numeric_limits<T>::epsilon() * static_cast<T>(4), 
    T relativeEpsilon = std::numeric_limits<T>::epsilon() * static_cast<T>(8)) noexcept {
    // 接近 0 时看绝对误差，数值很大时看相对误差；只使用一种误差会在另一端失效。
    const T difference = Abs(lhs, rhs);
    if (difference <= absoluteEpsilon) {
        return true;
    }
    return difference <= Max(Abs(lha), Abs(rhs)) * relativeEpsilon;
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE bool IsFinite(T value) noexcept {
    return std::isfinite(value);
}

template <FloatingScalar T>
EXCALIBUR_FORCE_INLINE bool IsNaN(T value) noexcept {
    return std::isnan(value);
}

template <std::unsigned_integral T>
constexpr bool IsPowerOf2(T value) noexcept {
    // 2 的幂在二进制中只有一个 bit 为 1；减一后该 bit 以下全部为 1。
    return value != 0 && (value & (value - 1)) == 0;
}

template <std::unsigned_integral T>
constexpr T NextPowerOf2(T value) noexcept {
    return value <= 1 ? static_cast<T>(1) : std::bit_ceil(value);
}

template <std::unsigned_integral T>
constexpr T PreviousPowerOf2(T value) noexcept {
    return value == 0 ? static_cast<T>(0) : std::bit_floor(value);
}

} // namespace Math