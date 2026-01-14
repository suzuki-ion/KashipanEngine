#include "Vector2.h"
#include "Math/Vector2.h"
#include <cassert>
#include <algorithm>
#include <cmath>

namespace KashipanEngine {
namespace MathUtils {

Vector2 Lerp(const Vector2 &start, const Vector2 &end, float t) noexcept {
    return start * (1.0f - t) + end * t;
}

Vector2 Slerp(const Vector2 &start, const Vector2 &end, float t) noexcept {
    Vector2 normalizedStart = Normalize(start);
    Vector2 normalizedEnd = Normalize(end);
    float angle = std::acos(Dot(normalizedStart, normalizedEnd));
    float sinTheta = std::sin(angle);

    float t1 = std::sin(angle * (1.0f - t));
    float t2 = std::sin(angle * t);

    Vector2 result = (normalizedStart * t1 + normalizedEnd * t2) / sinTheta;
    return Normalize(result);
}

Vector2 Bezier(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, float t) noexcept {
    Vector2 p01 = Lerp(p0, p1, t);
    Vector2 p12 = Lerp(p1, p2, t);
    return Lerp(p01, p12, t);
}

Vector2 CatmullRomInterpolation(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, const Vector2 &p3, float t) noexcept {
    const float s = 0.5f;

    float t2 = t * t;
    float t3 = t2 * t;

    auto e3 = (-p0 + (3.0f * p1) - (3.0f * p2) + p3) * t3;
    auto e2 = ((2.0f * p0) - (5.0f * p1) + (4.0f * p2) - p3) * t2;
    auto e1 = (-p0 + p2) * t;
    auto e0 = 2.0f * p1;

    return s * (e3 + e2 + e1 + e0);
}

Vector2 CatmullRomPosition(const std::vector<Vector2> &points, float t, bool isLoop) {
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

constexpr float Dot(const Vector2 &vector1, const Vector2 &vector2) noexcept {
    return vector1.x * vector2.x + vector1.y * vector2.y;
}

constexpr float Cross(const Vector2 &vector1, const Vector2 &vector2) noexcept {
    return vector1.x * vector2.y - vector1.y * vector2.x;
}

float Length(const Vector2 &vector) noexcept {
    return std::sqrt(LengthSquared(vector));
}

constexpr float LengthSquared(const Vector2 &vector) noexcept {
    return Dot(vector, vector);
}

Vector2 Normalize(const Vector2 &vector) {
    const float len = Length(vector);
    return (len != 0.0f) ? vector / len : Vector2(0.0f);
}

Vector2 Projection(const Vector2 &vector, const Vector2 &onto) noexcept {
    const float denom = Dot(onto, onto);
    return (denom != 0.0f) ? (Dot(vector, onto) / denom) * onto : Vector2(0.0f);
}

Vector2 Rejection(const Vector2 &vector, const Vector2 &onto) noexcept {
    return vector - Projection(vector, onto);
}

Vector2 Perpendicular(const Vector2 &vector) noexcept {
    return Vector2(-vector.y, vector.x);
}

Vector2 Reflection(const Vector2 &vector, const Vector2 &normal) noexcept {
    return vector - 2.0f * Dot(vector, normal) * normal;
}

float Distance(const Vector2 &vector1, const Vector2 &vector2) noexcept {
    return Length(vector1 - vector2);
}

} // namespace MathUtils
} // namespace KashipanEngine