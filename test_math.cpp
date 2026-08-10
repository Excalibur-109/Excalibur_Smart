#include "Math.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <numeric>
#include <random>
#include <span>
#include <string>

namespace {

int gFailures = 0;

void Check(bool condition, const char* name) {
    if (!condition) {
        ++gFailures;
        std::cerr << "FAIL: " << name << '\n';
    }
}

template <typename T>
bool Near(T lhs, T rhs, T epsilon = static_cast<T>(1e-4)) {
    return std::abs(lhs - rhs) <= epsilon;
}

template <Math::FloatingScalar T, std::size_t N>
bool NearVector(const Math::Vector<T, N>& lhs, const Math::Vector<T, N>& rhs,
                T epsilon = static_cast<T>(1e-4)) {
    for (std::size_t index = 0; index < N; ++index) {
        if (!Near(lhs[index], rhs[index], epsilon)) {
            return false;
        }
    }
    return true;
}

template <Math::FloatingScalar T, std::size_t R, std::size_t C>
bool NearMatrix(const Math::Matrix<T, R, C>& lhs, const Math::Matrix<T, R, C>& rhs,
                T epsilon = static_cast<T>(1e-4)) {
    for (std::size_t row = 0; row < R; ++row) {
        if (!NearVector(lhs[row], rhs[row], epsilon)) {
            return false;
        }
    }
    return true;
}

template <Math::FloatingScalar T, std::size_t N>
bool IsFinite(const Math::Vector<T, N>& value) {
    for (std::size_t index = 0; index < N; ++index) {
        if (!std::isfinite(value[index])) {
            return false;
        }
    }
    return true;
}

template <Math::FloatingScalar T, std::size_t R, std::size_t C>
bool IsFinite(const Math::Matrix<T, R, C>& value) {
    for (std::size_t row = 0; row < R; ++row) {
        if (!IsFinite(value[row])) {
            return false;
        }
    }
    return true;
}

void TestScalar() {
    using namespace Math;

    Check(Near(PI<float>, std::numbers::pi_v<float>), "Scalar.PI");
    Check(Near(TwoPI<float>, 2.0F * PI<float>), "Scalar.TwoPI");
    Check(Near(HalfPI<float>, PI<float> * 0.5F), "Scalar.HalfPI");
    Check(Near(QuarterPI<float>, PI<float> * 0.25F), "Scalar.QuarterPI");
    Check(Near(InvPI<float>, 1.0F / PI<float>), "Scalar.InvPI");
    Check(Near(SqrtTwo<float>, std::sqrt(2.0F)), "Scalar.SqrtTwo");
    Check(GoldenRadio<float> > 1.6F && GoldenRadio<float> < 1.7F, "Scalar.GoldenRadio");
    Check(Min(4, 2) == 2 && Max(4, 2) == 4, "Scalar.MinMax");
    Check(Clamp(8, 0, 5) == 5 && Clamp(-2, 0, 5) == 0, "Scalar.Clamp");
    Check(Near(Saturate(-0.5F), 0.0F) && Near(Saturate(1.5F), 1.0F), "Scalar.Saturate");
    Check(Near(Lerp(2.0F, 6.0F, 0.25F), 3.0F), "Scalar.Lerp.same-type");
    Check(Near(Lerp(2, 6, 0.25F), 3.0F), "Scalar.Lerp.mixed-type");
    Check(Near(LerpClamped(2.0F, 6.0F, 2.0F), 6.0F), "Scalar.LerpClamped");
    Check(Near(InverseLerp(2.0F, 6.0F, 3.0F), 0.25F), "Scalar.InverseLerp");
    Check(Near(Remap(0.0F, 1.0F, 10.0F, 30.0F, 0.25F), 15.0F), "Scalar.Remap");
    Check(Square(3) == 9 && Cube(3) == 27 && Abs(-3) == 3 && Abs(3U) == 3U,
          "Scalar.PowerAbs");
    Check(Sign(-2) == -1 && Sign(0) == 0 && Sign(2) == 1, "Scalar.Sign");
    Check(Near(Degrees(PI<float>), 180.0F) && Near(Radians(180.0F), PI<float>),
          "Scalar.AngleConversions");
    Check(Near(Fract(-1.25F), 0.75F) && Near(Mod(5.5F, 2.0F), 1.5F) && Mod(7, 3) == 1,
          "Scalar.FractMod");
    Check(Near(Repeat(-0.5F, 2.0F), 1.5F) && Near(Wrap(5.0F, -1.0F, 1.0F), -1.0F) &&
              Near(PingPong(3.0F, 2.0F), 1.0F),
          "Scalar.RepeatWrapPingPong");
    Check(Step(2.0F, 1.0F) == 0.0F && Step(2.0F, 2.0F) == 1.0F, "Scalar.Step");
    Check(Near(SmoothStep(0.0F, 1.0F, 0.5F), 0.5F) &&
              Near(SmootherStep(0.0F, 1.0F, 0.5F), 0.5F),
          "Scalar.SmoothSteps");
    Check(NearlyEqual(1.0F, 1.0F + std::numeric_limits<float>::epsilon()), "Scalar.NearlyEqual");
    Check(Math::IsFinite(1.0F) && !Math::IsFinite(std::numeric_limits<float>::infinity()) &&
              Math::IsNaN(std::numeric_limits<float>::quiet_NaN()),
          "Scalar.FiniteNaN");
    Check(IsPowerOf2(1U) && IsPowerOf2(1024U) && !IsPowerOf2(0U) && !IsPowerOf2(3U),
          "Scalar.IsPowerOf2");
    Check(NextPowerOf2(9U) == 16U && PreviousPowerOf2(9U) == 8U &&
              AlignDown(13U, 8U) == 8U && AlignUp(13U, 8U) == 16U &&
              AlignUp(13U, 0U) == 13U,
          "Scalar.PowerAlignment");
    Check(Select(true, 3, 4) == 3 && Select(false, 3, 4) == 4, "Scalar.Select");
}

void TestVectorAndFunctions() {
    using namespace Math;

    const float2 v2(1.0F, 2.0F);
    const float3 v3(1.0F, 2.0F, 3.0F);
    const float4 v4(v3, 4.0F);
    Check(v2.r() == 1.0F && v2.g() == 2.0F && v3.b() == 3.0F && v4.a() == 4.0F,
          "Vector.ComponentAliases");
    Check(v2.yx() == float2(2.0F, 1.0F) && v3.zxy() == float3(3.0F, 1.0F, 2.0F) &&
              v4.wzyx() == float4(4.0F, 3.0F, 2.0F, 1.0F),
          "Vector.NamedSwizzles");
    Check(v4.Swizzle<3, 2, 1, 0>() == float4(4.0F, 3.0F, 2.0F, 1.0F),
          "Vector.TemplateSwizzle");
    float3 setSwizzle(1.0F, 2.0F, 3.0F);
    setSwizzle.SetSwizzle<2, 0>(float2(9.0F, 8.0F));
    Check(setSwizzle == float3(8.0F, 2.0F, 9.0F), "Vector.SetSwizzle");
    Check(Vector<int, 3>(v3) == int3(1, 2, 3) && float3(float4(1, 2, 3, 4)) == v3,
          "Vector.Conversions");

    const int3 ia(6, 4, 2);
    const int3 ib(1, 2, 3);
    Check(ia + ib == int3(7, 6, 5) && ia - ib == int3(5, 2, -1) && ia * ib == int3(6, 8, 6) &&
              ia / ib == int3(6, 2, 0),
          "Vector.BinaryArithmetic");
    Check(ia + 2 == int3(8, 6, 4) && 2 + ia == int3(8, 6, 4) && -ia == int3(-6, -4, -2),
          "Vector.ScalarArithmetic");
    int3 compound = ia;
    compound += ib;
    compound -= 1;
    compound *= 2;
    compound /= 2;
    Check(compound == int3(6, 5, 4), "Vector.CompoundArithmetic");
    Check((ia % ib) == int3(0, 0, 2) && (ia & ib) == int3(0, 0, 2) &&
              (ia | ib) == int3(7, 6, 3) && (ia ^ ib) == int3(7, 6, 1) &&
              (~int3(0, 1, 2)) == int3(~0, ~1, ~2),
          "Vector.IntegralOperators");
    const bool3 mask(true, false, true);
    Check(Any(mask) && !All(mask) && !None(mask) && None(bool3(false, false, false)) &&
              (~mask) == bool3(false, true, false),
          "Vector.BooleanReductions");
    Check(Equal(ia, ib) == bool3(false, false, false) && Less(ib, ia) == bool3(true, true, false) &&
              GreaterEqual(ia, ib) == bool3(true, true, false),
          "Vector.Comparisons");
    Check(ToArray(v3) == std::array<float, 3>{1.0F, 2.0F, 3.0F} &&
              FromArray(std::array<float, 3>{4.0F, 5.0F, 6.0F}) == float3(4.0F, 5.0F, 6.0F),
          "Vector.ArrayConversions");
    Check(Near(Dot(v3, float3(4.0F, 5.0F, 6.0F)), 32.0F) &&
              Cross(float3(1, 0, 0), float3(0, 1, 0)) == float3(0, 0, 1),
          "Vector.DotCross");
    Check(Near(LengthSquared(v3), 14.0F) && Near(Length(v3), std::sqrt(14.0F)) &&
              Near(Distance(v3, float3(1, 2, 6)), 3.0F),
          "Vector.LengthDistance");
    Check(NearVector(Normalize(float3(0, 3, 4)), float3(0, 0.6F, 0.8F)) &&
              Normalize(int2(3, 4)) == Vector<double, 2>(0.6, 0.8) &&
              NormalizeSafe(float3(0), float3(1, 0, 0)) == float3(1, 0, 0),
          "Vector.Normalize");
    Check(Abs(int3(-1, 2, -3)) == int3(1, 2, 3) &&
              Min(int3(1, 6, 3), int3(2, 5, 4)) == int3(1, 5, 3) &&
              Max(int3(1, 6, 3), int3(2, 5, 4)) == int3(2, 6, 4) &&
              Clamp(int3(-1, 2, 5), int3(0), int3(3)) == int3(0, 2, 3) &&
              Saturate(float3(-1, 0.5F, 2)) == float3(0, 0.5F, 1),
          "Vector.ComponentMath");
    Check(Lerp(float3(0), float3(10), 0.25F) == float3(2.5F) &&
              Floor(float3(1.2F, -1.2F, 2.8F)) == float3(1, -2, 2) &&
              Ceil(float3(1.2F, -1.2F, 2.8F)) == float3(2, -1, 3) &&
              Round(float3(1.2F, -1.8F, 2.5F)) == float3(1, -2, 3) &&
              NearVector(Fract(float3(-1.2F, 1.2F, 2.8F)), float3(0.8F, 0.2F, 0.8F)),
          "Vector.Rounding");
    Check(SmoothStep(float3(0), float3(1), float3(0.5F)) == float3(0.5F) &&
              Reflect(float3(1, -1, 0), float3(0, 1, 0)) == float3(1, 1, 0) &&
              NearVector(Refract(float3(0, -1, 0), float3(0, 1, 0), 0.5F), float3(0, -1, 0)) &&
              Refract(float3(1, 0, 0), float3(0, 1, 0), 2.0F) == float3(0),
          "Vector.Optics");
    Check(Project(float3(2, 2, 0), float3(1, 0, 0)) == float3(2, 0, 0) &&
              Reject(float3(2, 2, 0), float3(1, 0, 0)) == float3(0, 2, 0) &&
              Near(AngleBetween(float3(1, 0, 0), float3(0, 1, 0)), HalfPI<float>),
          "Vector.ProjectRejectAngle");

    const float3 trig(0.0F, HalfPI<float>, PI<float>);
    Check(NearVector(Sin(trig), float3(0, 1, 0)) && NearVector(Cos(trig), float3(1, 0, -1)) &&
              IsFinite(Tan(float3(0.1F, 0.2F, 0.3F))) &&
              NearVector(Asin(float3(0, 0.5F, -0.5F)), float3(0, PI<float> / 6.0F, -PI<float> / 6.0F)) &&
              NearVector(Acos(float3(1, 0, -1)), float3(0, HalfPI<float>, PI<float>)) &&
              NearVector(Atan(float3(0, 1, -1)), float3(0, PI<float> / 4.0F, -PI<float> / 4.0F)),
          "Functions.Trigonometry");
    Check(NearVector(Atan2(float3(0, 1, -1), float3(1, 1, 1)), float3(0, PI<float> / 4.0F, -PI<float> / 4.0F)) &&
              NearVector(Sqrt(float3(1, 4, 9)), float3(1, 2, 3)) &&
              NearVector(ReciprocalSqrt(float3(1, 4, 9)), float3(1, 0.5F, 1.0F / 3.0F)),
          "Functions.Atan2Sqrt");
    Check(NearVector(Log(Exp(float3(0.1F, 0.2F, 0.3F))), float3(0.1F, 0.2F, 0.3F)) &&
              NearVector(Log2(Exp2(float3(1, 2, 3))), float3(1, 2, 3)) &&
              NearVector(Log10(float3(1, 10, 100)), float3(0, 1, 2)) &&
              Pow(float3(2, 3, 4), float3(2, 2, 2)) == float3(4, 9, 16) &&
              Pow(float3(2, 3, 4), 2.0F) == float3(4, 9, 16) &&
              Trunc(float3(-1.7F, 1.7F, 2.2F)) == float3(-1, 1, 2) &&
              Mod(float3(5.5F), float3(2.0F)) == float3(1.5F),
          "Functions.ExponentialLogarithm");
    Check(NearVector(Radians(float3(180)), float3(PI<float>)) &&
              NearVector(Degrees(float3(PI<float>)), float3(180)) &&
              Sign(float3(-1, 0, 1)) == int3(-1, 0, 1) && Sum(int3(1, 2, 3)) == 6 &&
              Product(int3(1, 2, 3)) == 6 && MinComponent(int3(3, 1, 2)) == 1 &&
              MaxComponent(int3(3, 1, 2)) == 3 && DistanceSquared(int3(1, 2, 3), int3(1, 2, 6)) == 9,
          "Functions.Reduction");
    Check(Select(bool3(true, false, true), int3(1, 2, 3), int3(4, 5, 6)) == int3(1, 5, 3) &&
              FaceForward(float3(0, 1, 0), float3(0, -1, 0), float3(0, 1, 0)) == float3(0, 1, 0) &&
              Perpendicular(float2(1, 2)) == float2(-2, 1) &&
              NearVector(Rotate(float2(1, 0), HalfPI<float>), float2(0, 1)),
          "Functions.SelectionAndRotation");
    const float3 cartesian = SphericalToCartesian(2.0F, HalfPI<float>, 0.0F);
    Check(NearVector(cartesian, float3(2, 0, 0)) &&
              NearVector(CartesianToSpherical(cartesian), float3(2, HalfPI<float>, 0)),
          "Functions.SphericalCoordinates");
}

void TestMatrix() {
    using namespace Math;

    Matrix<float, 2, 3> matrix(1, 2, 3, 4, 5, 6);
    const std::array<float2, 2> rows{float2(1, 2), float2(3, 4)};
    const Matrix<float, 2, 2> fromRows(rows);
    const Matrix<int, 2, 2> converted(fromRows);
    Check(matrix.Rows == 2 && matrix.Columns == 3 && matrix[1][2] == 6.0F,
          "Matrix.ConstructionAndIndexing");
    Check(fromRows == Matrix<float, 2, 2>(1, 2, 3, 4) && converted == Matrix<int, 2, 2>(1, 2, 3, 4),
          "Matrix.ArrayAndCastConstruction");
    Check(matrix.Column(1) == float2(2, 5), "Matrix.Column");
    matrix.SetColumn(2, float2(7, 8));
    Check(matrix[0][2] == 7.0F && matrix[1][2] == 8.0F, "Matrix.SetColumn");
    const Matrix<float, 2, 2> a(1, 2, 3, 4);
    const Matrix<float, 2, 2> b(5, 6, 7, 8);
    Check(Matrix<float, 2, 2>::Identity() == Matrix<float, 2, 2>(1, 0, 0, 1) &&
              a + b == Matrix<float, 2, 2>(6, 8, 10, 12) &&
              b - a == Matrix<float, 2, 2>(4, 4, 4, 4) && -a == Matrix<float, 2, 2>(-1, -2, -3, -4),
          "Matrix.BasicArithmetic");
    Check(a * 2.0F == Matrix<float, 2, 2>(2, 4, 6, 8) && 2.0F * a == Matrix<float, 2, 2>(2, 4, 6, 8) &&
              b / 2.0F == Matrix<float, 2, 2>(2.5F, 3, 3.5F, 4),
          "Matrix.ScalarArithmetic");
    Matrix<float, 2, 2> compound = a;
    compound += b;
    compound -= b;
    compound *= 2.0F;
    compound /= 2.0F;
    Check(compound == a, "Matrix.CompoundArithmetic");
    Check(a * float2(1, 2) == float2(5, 11) &&
              a * b == Matrix<float, 2, 2>(19, 22, 43, 50),
          "Matrix.Multiplication");
    Check(Transpose(Matrix<float, 2, 3>(1, 2, 3, 4, 5, 6)) == Matrix<float, 3, 2>(1, 4, 2, 5, 3, 6) &&
              Hadamard(a, b) == Matrix<float, 2, 2>(5, 12, 21, 32) && Trace(a) == 5.0F &&
              Determinant(Matrix<float, 2, 2>(1, 2, 3, 4)) == -2.0F &&
              Determinant(Matrix<float, 3, 3>(1, 2, 3, 0, 1, 4, 5, 6, 0)) == 1.0F &&
              Determinant(Matrix<float, 4, 4>::Identity()) == 1.0F,
          "Matrix.Reduction");
    Matrix<float, 2, 2> inverseOut{};
    const Matrix<float, 2, 2> invertible(4, 7, 2, 6);
    Check(TryInverse(invertible, &inverseOut) && NearMatrix(invertible * inverseOut, Matrix<float, 2, 2>::Identity()) &&
              !TryInverse(invertible, static_cast<Matrix<float, 2, 2>*>(nullptr)) &&
              !TryInverse(Matrix<float, 2, 2>(1, 2, 2, 4), &inverseOut) &&
              Inverse(invertible).has_value(),
          "Matrix.Inverse");
    Check(MatrixCast<double>(a) == Matrix<double, 2, 2>(1, 2, 3, 4) &&
              ResizeMatrix<float, 4, 4>(Matrix<float, 3, 3>::Identity()) == Matrix<float, 4, 4>::Identity(),
          "Matrix.CastResize");

    const float3 translation(3, 4, 5);
    const float3 scale(2, 3, 4);
    const float4x4 translate = TranslationMatrix(translation);
    const float4x4 scaling = ScaleMatrix(scale);
    Check(TransformPoint(translate, float3(1, 2, 3)) == float3(4, 6, 8) &&
              TransformVector(translate, float3(1, 2, 3)) == float3(1, 2, 3) &&
              TransformVector(scaling, float3(1, 1, 1)) == scale,
          "Matrix.Transforms");
    const float4x4 rotations = RotationXMatrix(0.1F) * RotationYMatrix(0.2F) * RotationZMatrix(0.3F) *
                              RotationAxisMatrix(float3(0, 1, 0), 0.4F);
    const float4x4 viewRH = LookAtRH(float3(0, 0, 3), float3(0), float3(0, 1, 0));
    const float4x4 viewLH = LookAtLH(float3(0, 0, -3), float3(0), float3(0, 1, 0));
    const float4x4 projectionRH = PerspectiveRH_ZO(1.0F, 1.5F, 0.1F, 100.0F);
    const float4x4 projectionLH = PerspectiveLH_ZO(1.0F, 1.5F, 0.1F, 100.0F);
    const float4x4 projectionNO = PerspectiveRH_NO(1.0F, 1.5F, 0.1F, 100.0F);
    const float4x4 projectionVk = PerspectiveVulkanRH_ZO(1.0F, 1.5F, 0.1F, 100.0F);
    Check(IsFinite(rotations) && IsFinite(viewRH) && IsFinite(viewLH) && IsFinite(projectionRH) &&
              IsFinite(projectionLH) && IsFinite(projectionNO) && IsFinite(projectionVk) &&
              IsFinite(OrthographicRH_ZO(-1.0F, 1.0F, -1.0F, 1.0F, 0.1F, 100.0F)) &&
              IsFinite(OrthographicLH_ZO(-1.0F, 1.0F, -1.0F, 1.0F, 0.1F, 100.0F)),
          "Matrix.CameraFactories");
}

void TestColor() {
    using namespace Math;

    const float3 linear(0.25F, 0.5F, 0.75F);
    Check(Near(LinearToSRGB(SRGBToLinear(0.5F)), 0.5F, 1e-5F) &&
              NearVector(LinearToSRGB(SRGBToLinear(linear)), linear, 1e-5F),
          "Color.SRGBConversions");
    Check(Near(Luminance(float3(1, 1, 1)), 1.0F) &&
              ApplyExposure(float3(1), 1.0F) == float3(2) &&
              ToneMapReinhard(float3(1)) == float3(0.5F) &&
              IsFinite(ToneMapReinhardExtended(float3(2), 1.0F)) &&
              IsFinite(ToneMapACES(float3(2))),
          "Color.Lighting");
    const float3 hsv = RGBToHSV(float3(0.2F, 0.7F, 0.4F));
    Check(NearVector(HSVToRGB(hsv), float3(0.2F, 0.7F, 0.4F), 1e-4F), "Color.HSV");
    const float4 alpha(0.8F, 0.4F, 0.2F, 0.5F);
    Check(PremultiplyAlpha(alpha) == float4(0.4F, 0.2F, 0.1F, 0.5F) &&
              UnpremultiplyAlpha(PremultiplyAlpha(alpha)) == alpha &&
              UnpremultiplyAlpha(float4(1, 2, 3, 0)) == float4(0),
          "Color.Alpha");
    const float4 packedInput(0.0F, 0.5F, 1.0F, 1.0F);
    Check(NearVector(UnpackRGBA8UNorm(PackRGBA8UNorm(packedInput)), packedInput, 1.0F / 255.0F),
          "Color.Packing");
}

void TestQuaternion() {
    using namespace Math;

    const floatQuaternion identity = floatQuaternion::Identity();
    const floatQuaternion rotation = QuaternionFromAxisAngle(float3(0, 1, 0), HalfPI<float>);
    const floatQuaternion fromVector(float4(0, 0, 0, 1));
    const doubleQuaternion widened(rotation);
    Check(IsIdentity(identity) && IsUnit(rotation) && IsNormalized(rotation) &&
              NearVector(rotation.xyzw(), float4(0, std::sqrt(0.5F), 0, std::sqrt(0.5F))) &&
              fromVector == identity && Near(widened.y, static_cast<double>(rotation.y)),
          "Quaternion.Construction");
    Check(-identity == floatQuaternion(0, 0, 0, -1) &&
              identity + identity == floatQuaternion(0, 0, 0, 2) &&
              Near(Length(rotation), 1.0F) && NearlyEqual(Conjugate(rotation), Inverse(rotation), 1e-5F),
          "Quaternion.BasicOperations");
    Check(NearVector(Rotate(rotation, float3(1, 0, 0)), float3(0, 0, -1)) &&
              NearVector(Matrix3x3FromQuaternion(rotation) * float3(1, 0, 0), float3(0, 0, -1)) &&
              NearMatrix(Matrix4x4FromQuaternion(rotation), ResizeMatrix<float, 4, 4>(Matrix3x3FromQuaternion(rotation))),
          "Quaternion.RotationAndMatrix");
    const floatQuaternion fromMatrix = QuaternionFromMatrix(Matrix3x3FromQuaternion(rotation));
    Check(EqualsRotation(rotation, fromMatrix) &&
              IsUnit(Nlerp(identity, rotation, 0.5F)) && IsUnit(Slerp(identity, rotation, 0.5F)),
          "Quaternion.ConversionsAndInterpolation");
    const float4x4 trs = TRSMatrix(float3(1, 2, 3), rotation, float3(2));
    Check(NearVector(TransformPoint(trs, float3(1, 0, 0)), float3(1, 2, 1)) &&
              NearlyEqual(rotation, rotation) && EqualsRotation(rotation, -rotation),
          "Quaternion.TRSAndEquality");
    floatQuaternion compound = identity;
    compound += identity;
    compound -= identity;
    compound *= rotation;
    compound *= 2.0F;
    compound /= 2.0F;
    Check(EqualsRotation(compound, rotation), "Quaternion.CompoundOperators");
    float3 axis{};
    float angle = 0.0F;
    QuaternionToAxisAngle(rotation, axis, angle);
    const float3 euler = QuaternionToEulerXYZ(QuaternionFromEulerXYZ(float3(0.1F, 0.2F, 0.3F)));
    Check(NearVector(axis, float3(0, 1, 0)) && Near(angle, HalfPI<float>) &&
              NearVector(euler, float3(0.1F, 0.2F, 0.3F), 1e-4F),
          "Quaternion.AxisAngleEuler");
    Check(NearVector(Rotate(FromToRotation(float3(1, 0, 0), float3(0, 1, 0)), float3(1, 0, 0)), float3(0, 1, 0)) &&
              Near(AngleBetween(identity, rotation), HalfPI<float>) &&
              NearVector(GetRight(identity), float3(1, 0, 0)) &&
              NearVector(GetUp(identity), float3(0, 1, 0)) &&
              NearVector(GetForward(identity), float3(0, 0, -1)),
          "Quaternion.Directions");
    const floatQuaternion logExp = QuaternionExp(QuaternionLog(rotation));
    const floatQuaternion control = QuaternionSquadControl(identity, rotation, QuaternionFromAxisAngle(float3(1, 0, 0), 0.5F));
    Check(EqualsRotation(rotation, logExp, 1e-4F) && IsUnit(control) &&
              IsUnit(Squad(identity, rotation, control, control, 0.5F)),
          "Quaternion.LogExpSquad");
}

void TestRandom() {
    using namespace Math;

    SplitMix64 splitA(42);
    SplitMix64 splitB(42);
    XorShift64Star xorA(42);
    XorShift64Star xorB(42);
    Pcg32 pcgA(42, 7);
    Pcg32 pcgB(42, 7);
    Check(splitA.NextU64() == splitB.NextU64() && xorA.NextU64() == xorB.NextU64() &&
              pcgA.NextU32() == pcgB.NextU32() && pcgA.NextU64() == pcgB.NextU64(),
          "Random.GeneratorDeterminism");
    Check(Hash32(42) == Hash32(42) && Hash64(42) == Hash64(42) && Hash32(42) != Hash32(43) &&
              HashFloat01(42) >= 0.0F && HashFloat01(42) < 1.0F,
          "Random.Hashing");

    Random random(123, 9);
    Random repeat(123, 9);
    Check(random.UInt() == repeat.UInt() && random.UInt64() == repeat.UInt64(), "Random.Seeding");
    random.Seed(123, 9);
    Check(random.UInt(0U) == 0U && random.UInt64(0U) == 0U && random.UInt(5U, 5U) == 5U &&
              random.Int(-3, -3) == -3 && random.Float01() >= 0.0F && random.Float01() < 1.0F &&
              random.Double01() >= 0.0 && random.Double01() < 1.0,
          "Random.Ranges");
    const bool randomBoolean = random.Bool();
    static_cast<void>(randomBoolean);
    Check(random.Float(-2.0F, 3.0F) >= -2.0F && random.Float(-2.0F, 3.0F) < 3.0F &&
              random.Double(-2.0, 3.0) >= -2.0 && random.Double(-2.0, 3.0) < 3.0 &&
              !random.Chance(0.0F) && random.Chance(1.0F),
          "Random.Primitives");
    Check(std::isfinite(random.Normal()) && random.Exponential(0.0) == 0.0 && random.Exponential(2.0) >= 0.0 &&
              IsFinite(random.VectorRange(float3(-1), float3(1))) &&
              Near(Length(random.OnUnitCircle()), 1.0F, 1e-4F) && Length(random.InsideUnitCircle()) <= 1.0F + 1e-4F &&
              Near(Length(random.OnUnitSphere()), 1.0F, 1e-4F) && Length(random.InsideUnitSphere()) <= 1.0F + 1e-4F,
          "Random.Distributions");
    Check(Dot(random.OnHemisphere(float3(0, 1, 0)), float3(0, 1, 0)) >= 0.0F &&
              Near(Length(random.CosineWeightedHemisphere(float3(0, 1, 0))), 1.0F, 1e-4F) &&
              IsUnit(random.UniformQuaternion()) &&
              Near(Sum(random.TriangleBarycentric()), 1.0F, 1e-5F),
          "Random.Geometry");
    std::array<int, 5> values{1, 2, 3, 4, 5};
    random.Shuffle(std::span<int>(values));
    Check(std::accumulate(values.begin(), values.end(), 0) == 15, "Random.Shuffle");
    const auto unique = random.UniqueString(4, "aabcde");
    Check(unique.has_value() && unique->size() == 4 && !random.UniqueString(7, "abc").has_value() &&
              random.UniqueString(0).value_or("x").empty(),
          "Random.UniqueString");
    const std::array<int, 3> choices{4, 5, 6};
    Check(random.Choose(std::span<const int>(choices)) != nullptr &&
              random.Choose(std::span<int>{}) == nullptr &&
              random.WeightedIndex(std::array<float, 3>{0, 0, 0}) == 3 &&
              random.WeightedIndex(std::array<float, 3>{0, 1, 0}) == 1,
          "Random.ChoiceAndWeights");
}

void TestRendering() {
    using namespace Math;

    const float3 normal(0, 0, 1);
    const TangentFrame<float> frame = BuildTangentFrame(normal);
    const TangentFrame<float> signedFrame = BuildTangentFrame(normal, float4(1, 0, 0, -1));
    Check(Near(Dot(frame.tangent, frame.normal), 0.0F) && Near(Dot(frame.bitangent, frame.normal), 0.0F) &&
              NearVector(WorldToTangent(frame, TangentToWorld(frame, float3(0.2F, 0.3F, 1.0F))),
                         Normalize(float3(0.2F, 0.3F, 1.0F)), 1e-4F) &&
              signedFrame.bitangent.y < 0.0F,
          "Rendering.TangentFrame");
    const float3 encodedNormal(0.5F, 0.5F, 1.0F);
    Check(NearVector(DecodeNormalMap(encodedNormal, frame), normal) &&
              NearVector(DecodeNormalOctahedral(EncodeNormalOctahedral(float3(0.2F, -0.3F, 0.9F))),
                         Normalize(float3(0.2F, -0.3F, 0.9F)), 1e-4F),
          "Rendering.NormalEncoding");
    const float4x4 transform = TranslationMatrix(float3(1, 2, 3)) * ScaleMatrix(float3(2, 3, 4));
    Check(NormalMatrix(transform).has_value() && Near(Length(TransformNormal(transform, normal)), 1.0F) &&
              NearVector(LambertDiffuse(float3(1)), float3(InvPI<float>)) &&
              IsFinite(FresnelSchlick(0.5F, float3(0.04F))) &&
              IsFinite(FresnelSchlickRoughness(0.5F, float3(0.04F), 0.5F)) &&
              DistributionGGX(0.5F, 0.5F) > 0.0F && GeometrySchlickGGX(0.5F, 0.5F) > 0.0F &&
              GeometrySmith(0.5F, 0.5F, 0.5F) > 0.0F,
          "Rendering.BRDFPrimitives");
    const PBRMaterialSample<float> material{};
    Check(IsFinite(EvaluateCookTorrance(material, normal, normal, normal)) &&
              IsFinite(EvaluateBlinnPhong(float3(0.5F), float3(0.5F), 16.0F, normal, normal, normal)),
          "Rendering.BRDF");
    const DirectionalLightData<float> directional{};
    const PointLightData<float> point{float3(0, 0, 2), float3(1), 10.0F, 10.0F};
    const SpotLightData<float> spot{float3(0, 0, 2), float3(0, 0, -1), float3(1), 10.0F, 10.0F, 0.2F, 0.5F};
    const LightSample<float> directionalSample = SampleLight(directional);
    const LightSample<float> pointSample = SampleLight(point, float3(0));
    const LightSample<float> spotSample = SampleLight(spot, float3(0));
    Check(SmoothDistanceAttenuation(10.0F, 10.0F) == 0.0F &&
              std::isinf(directionalSample.distance) && pointSample.distance == 2.0F && spotSample.distance == 2.0F &&
              SpotConeAttenuation(float3(0, 0, -1), float3(0, 0, -1), 0.2F, 0.5F) == 1.0F &&
              IsFinite(EvaluatePBRLight(material, normal, normal, directionalSample)),
          "Rendering.Lights");
    Check(ShadowBias(0.0F, 0.001F, 0.01F, 0.005F) == 0.005F &&
              VarianceShadowVisibility(0.25F, 0.5F, 0.25F) == 1.0F &&
              FogTransmittanceLinear(0.0F, 0.0F, 10.0F) == 1.0F &&
              FogTransmittanceExponential(1.0F, 1.0F) < 1.0F &&
              FogTransmittanceExponentialSquared(1.0F, 1.0F) < 1.0F &&
              ApplyFog(float3(1), float3(0), 0.5F) == float3(0.5F),
          "Rendering.ShadowAndFog");
    const float4x4 projection = PerspectiveRH_ZO(1.0F, 1.0F, 0.1F, 100.0F);
    const auto inverseProjection = Inverse(projection);
    Check(std::isfinite(ReconstructViewZ(0.5F, projection)) && inverseProjection.has_value() &&
              IsFinite(ReconstructViewPosition(float2(0, 0), 0.5F, *inverseProjection)) &&
              IsFinite(ReconstructWorldPosition(float2(0, 0), 0.5F, *inverseProjection)) &&
              PixelCenterToNDC(float2(0, 0), float2(2, 2)) == float2(-0.5F, -0.5F) &&
              PixelCenterToNDC(float2(0, 0), float2(2, 2), true) == float2(-0.5F, 0.5F),
          "Rendering.Reconstruction");
    Check(ReverseBits32(1U) == 0x80000000U && Near(RadicalInverseVanDerCorput(1U), 0.5F) &&
              Hammersley2D(0, 0) == float2(0, 0) && Near(Length(CosineSampleHemisphere(float2(0.5F, 0.25F))), 1.0F) &&
              Near(Length(ImportanceSampleGGX(float2(0.5F, 0.5F), 0.5F, normal)), 1.0F),
          "Rendering.Sampling");
    SH9Color<float> coefficients{};
    coefficients[0] = float3(1, 2, 3);
    Check(SphericalHarmonicsBasis9(normal).size() == 9 && IsFinite(EvaluateSphericalHarmonics9(coefficients, normal)),
          "Rendering.SphericalHarmonics");
}

void TestCurves() {
    using namespace Math;

    const float p0 = 0.0F;
    const float p1 = 1.0F;
    const float p2 = 2.0F;
    const float p3 = 3.0F;
    std::array<float, 3> level{p0, p1, p2};
    Check(Near(detail::CurveLerp(p0, p1, 0.25F), 0.25F) &&
              detail::CurveLerp(float2(0), float2(2), 0.5F) == float2(1) &&
              detail::CurveLerp(float3(0), float3(2), 0.5F) == float3(1) &&
              detail::CurveLerp(float4(0), float4(2), 0.5F) == float4(1) &&
              Near(detail::EvaluateBezierLevel(level.data(), level.size(), 0.5F), 1.0F) &&
              Near(detail::CurveDistance(1.0F, 3.0F), 2.0F) &&
              Near(detail::CurveDistance(float2(0, 0), float2(3, 4)), 5.0F) &&
              Near(detail::KnotLerp(p0, p1, 0.0F, 1.0F, 0.25F), 0.25F) &&
              Near(detail::KnotLerp(p0, p1, 1.0F, 1.0F, 1.0F), p0) &&
              Near(LinearBezier(p0, p1, 0.5F), 0.5F) &&
              Near(QuadraticBezier(p0, p1, p2, 0.5F), 1.0F) &&
              Near(CubicBezier(p0, p1, p2, p3, 0.5F), 1.5F),
          "Curves.DetailAndBezierEvaluation");
    Check(Near(QuadraticBezierDerivative(p0, p1, p2, 0.5F), 2.0F) &&
              Near(QuadraticBezierSecondDerivative(p0, p1, p2), 0.0F) &&
              Near(CubicBezierDerivative(p0, p1, p2, p3, 0.5F), 3.0F) &&
              Near(CubicBezierSecondDerivative(p0, p1, p2, p3, 0.5F), 0.0F) &&
              Near(CubicBezierThirdDerivative(p0, p1, p2, p3), 0.0F),
          "Curves.BezierDerivatives");
    const std::array<float, 4> controls{p0, p1, p2, p3};
    const std::array<float, 4> weights{1, 1, 1, 1};
    Check(Near(Bezier<float>(std::span<const float>(controls), 0.5F), 1.5F) &&
              Near(Bezier(controls, 0.5F), 1.5F) && Near(BernsteinBasis<float>(3, 1, 0.5F), 0.375F) &&
              Near(BernsteinBasis<float>(3, 4, 0.5F), 0.0F) &&
              Near(RationalBezier<float>(std::span<const float>(controls), std::span<const float>(weights), 0.5F), 1.5F) &&
              Bezier<float>(std::span<const float>{}, 0.5F) == 0.0F &&
              RationalBezier<float>(std::span<const float>(controls), std::span<const float>{}, 0.5F) == 0.0F &&
              Near(RationalQuadraticBezier(p0, p1, p2, 1.0F, 1.0F, 1.0F, 0.5F), 1.0F),
          "Curves.BezierGeneral");
    const auto splitQuadratic = SplitQuadraticBezier(p0, p1, p2, 0.25F);
    const auto splitCubic = SplitCubicBezier(p0, p1, p2, p3, 0.25F);
    Check(Near(splitQuadratic[0][2], splitQuadratic[1][0]) && Near(splitCubic[0][3], splitCubic[1][0]),
          "Curves.BezierSplit");
    Check(Near(CubicHermite(p0, 1.0F, p1, 1.0F, 0.5F), 0.5F) &&
              Near(CubicHermiteDerivative(p0, 1.0F, p1, 1.0F, 0.5F), 1.0F) &&
              Near(CardinalSpline(p0, p1, p2, p3, 0.0F), p1) &&
              Near(CatmullRom(p0, p1, p2, p3, 1.0F), p2) &&
              Near(KochanekBartels(p0, p1, p2, p3, 0.5F, 0.0F, 0.0F, 0.0F), 1.5F) &&
              Near(CubicBSpline(p0, p1, p2, p3, 0.5F), 1.5F),
          "Curves.Splines");
    Check(IsFinite(CatmullRomNonUniform(float2(0, 0), float2(1, 1), float2(2, 1), float2(3, 0), 0.5F)) &&
              Near(Curvature(float2(1, 0), float2(0, 1)), 1.0F) &&
              Near(Curvature(float3(1, 0, 0), float3(0, 1, 0)), 1.0F) &&
              Near(ApproximateCurveLength([](float amount) { return float2(amount, 0.0F); }), 1.0F, 1e-5F),
          "Curves.Geometry");

    const float amount = 0.37F;
    Check(std::isfinite(EaseLinear(amount)) && std::isfinite(EaseInSine(amount)) && std::isfinite(EaseOutSine(amount)) &&
              std::isfinite(EaseInOutSine(amount)) && std::isfinite(EaseInPower(amount, 2.0F)) &&
              std::isfinite(EaseOutPower(amount, 2.0F)) && std::isfinite(EaseInOutPower(amount, 2.0F)) &&
              std::isfinite(EaseInQuadratic(amount)) && std::isfinite(EaseOutQuadratic(amount)) && std::isfinite(EaseInOutQuadratic(amount)) &&
              std::isfinite(EaseInCubic(amount)) && std::isfinite(EaseOutCubic(amount)) && std::isfinite(EaseInOutCubic(amount)) &&
              std::isfinite(EaseInQuartic(amount)) && std::isfinite(EaseOutQuartic(amount)) && std::isfinite(EaseInOutQuartic(amount)) &&
              std::isfinite(EaseInQuintic(amount)) && std::isfinite(EaseOutQuintic(amount)) && std::isfinite(EaseInOutQuintic(amount)) &&
              std::isfinite(EaseInExponential(amount)) && std::isfinite(EaseOutExponential(amount)) && std::isfinite(EaseInOutExponential(amount)) &&
              std::isfinite(EaseInCircular(amount)) && std::isfinite(EaseOutCircular(amount)) && std::isfinite(EaseInOutCircular(amount)) &&
              std::isfinite(EaseInBack(amount)) && std::isfinite(EaseOutBack(amount)) && std::isfinite(EaseInOutBack(amount)) &&
              std::isfinite(EaseInElastic(amount)) && std::isfinite(EaseOutElastic(amount)) && std::isfinite(EaseInOutElastic(amount)) &&
              std::isfinite(EaseInBounce(amount)) && std::isfinite(EaseOutBounce(amount)) && std::isfinite(EaseInOutBounce(amount)),
          "Curves.EasingFunctions");
    for (int curve = static_cast<int>(EaseCurve::Linear); curve <= static_cast<int>(EaseCurve::InOutBounce); ++curve) {
        Check(std::isfinite(EvaluateEase(static_cast<EaseCurve>(curve), amount)), "Curves.EvaluateEase");
    }
}

void RunStressTest() {
    using namespace Math;

    constexpr int iterations = 5000;
    std::mt19937 generator(0xC0FFEEU);
    std::uniform_real_distribution<float> values(-10.0F, 10.0F);
    std::uniform_real_distribution<float> angles(-PI<float>, PI<float>);
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const float3 value(values(generator), values(generator), values(generator));
        const float3 other(values(generator), values(generator), values(generator));
        const float3 unit = NormalizeSafe(value, float3(1, 0, 0));
        const float3 color = Saturate(value * 0.1F + 0.5F);
        Check(IsFinite(unit) && Near(Length(unit), 1.0F, 1e-4F) &&
                  IsFinite(Sin(value)) && IsFinite(Cos(value)) && IsFinite(Exp(value * 0.01F)) &&
                  IsFinite(HSVToRGB(RGBToHSV(color))) &&
                  IsFinite(CatmullRomNonUniform(value, other, value + 1.0F, other + 1.0F, 0.5F)),
              "Stress.VectorColorCurve");

        const floatQuaternion rotation = QuaternionFromAxisAngle(unit, angles(generator));
        const float3 scale(std::abs(values(generator)) + 0.1F, std::abs(values(generator)) + 0.1F,
                           std::abs(values(generator)) + 0.1F);
        const float4x4 transform = TRSMatrix(value, rotation, scale);
        const auto inverse = Inverse(transform);
        Check(IsUnit(rotation) && inverse.has_value() &&
                  NearMatrix(transform * *inverse, float4x4::Identity(), 2e-3F) &&
                  IsFinite(TransformNormal(transform, unit)) &&
                  NearVector(DecodeNormalOctahedral(EncodeNormalOctahedral(unit)), unit, 2e-4F),
              "Stress.MatrixQuaternionRendering");
    }
}

} // namespace

int main() {
    TestScalar();
    TestVectorAndFunctions();
    TestMatrix();
    TestColor();
    TestQuaternion();
    TestRandom();
    TestRendering();
    TestCurves();
    RunStressTest();

    if (gFailures == 0) {
        std::cout << "Math test suite passed: public API checks and 5000 stress iterations.\n";
        return EXIT_SUCCESS;
    }

    std::cerr << "Math test suite failed with " << gFailures << " failed checks.\n";
    return EXIT_FAILURE;
}
