#include "Vector3.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include <cassert>
#include <algorithm>
#include <cmath>

namespace KashipanEngine {
namespace MathUtils {

Vector3 Lerp(const Vector3 &start, const Vector3 &end, float t) noexcept {
    return start * (1.0f - t) + end * t;
}

Vector3 Slerp(const Vector3 &start, const Vector3 &end, float t) noexcept {
    Vector3 normalizedStart = Normalize(start);
    Vector3 normalizedEnd = Normalize(end);

    float dotProduct = Dot(normalizedStart, normalizedEnd);
    // Dotの値が変な値にならないよう制限
    dotProduct = std::clamp(dotProduct, -1.0f, 1.0f);
    float angle = std::acos(dotProduct);
    float sinTheta = std::sin(angle);
    // 角度が0の場合は線形補間を行う
    if (sinTheta == 0.0f) {
        return Normalize(Lerp(start, end, t));
    }

    float t1 = std::sin(angle * (1.0f - t));
    float t2 = std::sin(angle * t);

    Vector3 result = (normalizedStart * t1 + normalizedEnd * t2) / sinTheta;
    return Normalize(result);
}

Vector3 Bezier(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, float t) noexcept {
    Vector3 p01 = Lerp(p0, p1, t);
    Vector3 p12 = Lerp(p1, p2, t);
    return Lerp(p01, p12, t);
}

Vector3 CatmullRomInterpolation(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) noexcept {
    const float s = 0.5f;

    float t2 = t * t;
    float t3 = t2 * t;

    auto e3 = (-p0 + (3.0f * p1) - (3.0f * p2) + p3) * t3;
    auto e2 = ((2.0f * p0) - (5.0f * p1) + (4.0f * p2) - p3) * t2;
    auto e1 = (-p0 + p2) * t;
    auto e0 = 2.0f * p1;

    return s * (e3 + e2 + e1 + e0);
}

Vector3 CatmullRomPosition(const std::vector<Vector3> &points, float t, bool isLoop) {
    assert(points.size() >= 4);

    size_t division = isLoop ? points.size() : points.size() - 1;
    float areaWidth = 1.0f / static_cast<float>(division);
    size_t index    = static_cast<size_t>(t / areaWidth);
    index           = std::min(index, division - 1);
    float t2        = (t - areaWidth * static_cast<float>(index)) / areaWidth;
    t2              = std::clamp(t2, 0.0f, 1.0f);
     
    // インデックス取得関数（ループ対応）
    auto GetIndex = [&](int i) -> size_t {
        if (isLoop) {
            return (i + points.size()) % points.size();
        } else {
            return std::clamp(i, 0, static_cast<int>(points.size()) - 1);
        }
    };

    size_t index0 = GetIndex(static_cast<int>(index) - 1);
    size_t index1 = GetIndex(static_cast<int>(index));
    size_t index2 = GetIndex(static_cast<int>(index) + 1);
    size_t index3 = GetIndex(static_cast<int>(index) + 2);

    const auto &p0 = points[index0];
    const auto &p1 = points[index1];
    const auto &p2 = points[index2];
    const auto &p3 = points[index3];

    return CatmullRomInterpolation(p0, p1, p2, p3, t2);
}

constexpr float Dot(const Vector3 &vector1, const Vector3 &vector2) noexcept {
    return vector1.x * vector2.x + vector1.y * vector2.y + vector1.z * vector2.z;
}

Vector3 Cross(const Vector3 &vector1, const Vector3 &vector2) noexcept {
    return Vector3(
        vector1.y * vector2.z - vector1.z * vector2.y,
        vector1.z * vector2.x - vector1.x * vector2.z,
        vector1.x * vector2.y - vector1.y * vector2.x
    );
}

float Length(const Vector3 &vector) noexcept {
    return std::sqrt(LengthSquared(vector));
}

constexpr float LengthSquared(const Vector3 &vector) noexcept {
    return Dot(vector, vector);
}

Vector3 Normalize(const Vector3 &vector) {
    const float length = Length(vector);
    if (length == 0.0f) {
        return Vector3(0.0f, 0.0f, 0.0f);
    }
    return vector / length;
}

Vector3 Projection(const Vector3 &vector, const Vector3 &onto) noexcept {
    return (Dot(vector, onto) / Dot(onto, onto)) * onto;
}

Vector3 Perpendicular(const Vector3 &vector) noexcept {
    if (vector.x != 0.0f || vector.y != 0.0f) {
        return Vector3(-vector.y, vector.x, 0.0f);
    }
    return Vector3(0.0f, -vector.z, vector.y);
}

Vector3 Rejection(const Vector3 &vector, const Vector3 &onto) noexcept {
    return vector - Projection(vector, onto);
}

Vector3 Reflection(const Vector3 &vector, const Vector3 &normal) noexcept {
    return vector - 2.0f * Dot(vector, normal) * normal;
}

float Distance(const Vector3 &vector1, const Vector3 &vector2) {
    return Length(vector2 - vector1);
}

Vector3 Transform(const Vector3 &vector, const Matrix4x4 &mat) noexcept {
    Vector3 result{};
    result.x = vector.x * mat.m[0][0] + vector.y * mat.m[1][0] + vector.z * mat.m[2][0] + 1.0f * mat.m[3][0];
    result.y = vector.x * mat.m[0][1] + vector.y * mat.m[1][1] + vector.z * mat.m[2][1] + 1.0f * mat.m[3][1];
    result.z = vector.x * mat.m[0][2] + vector.y * mat.m[1][2] + vector.z * mat.m[2][2] + 1.0f * mat.m[3][2];
    float w = vector.x * mat.m[0][3] + vector.y * mat.m[1][3] + vector.z * mat.m[2][3] + 1.0f * mat.m[3][3];

    if (w == 0.0f) {
        return Vector3(0.0f);
    }

    result.x /= w;
    result.y /= w;
    result.z /= w;

    return result;
}

} // namespace MathUtils
} // namespace KashipanEngine