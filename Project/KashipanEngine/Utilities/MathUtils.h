#pragma once
#include <algorithm>
#include "MathUtils/Easings.h"
#include "MathUtils/Vector2.h"
#include "MathUtils/Vector3.h"
#include "MathUtils/Vector4.h"
#include "MathUtils/Matrix3x3.h"
#include "MathUtils/Matrix4x4.h"
#include "MathUtils/PerlinNoise.h"
#include "MathUtils/FractalNoise.h"
#include "MathUtils/SphericalCoordinates.h"

/// @brief 円周率を任意の型で取得する
template <typename T>
constexpr T GetPI() noexcept {
    return static_cast<T>(3.14159265358979323846);
}

template<typename T>
T ToDegrees(T radians) {
    static const T constant = static_cast<T>(180.0f / GetPI<T>());
    return radians * constant;
}
template <typename T>
T ToRadians(T degrees) {
    static const T constant = static_cast<T>(GetPI<T>() / 180.0f);
    return degrees * constant;
}

/// @brief 値を指定範囲内に収める
inline float Clamp(float value, float min, float max) {
    return std::clamp(value, min, max);
}
inline Vector2 Clamp(const Vector2 &value, const Vector2 &min, const Vector2 &max) {
    return Vector2(std::clamp(value.x, min.x, max.x), std::clamp(value.y, min.y, max.y));
}
inline Vector3 Clamp(const Vector3 &value, const Vector3 &min, const Vector3 &max) {
    return Vector3(std::clamp(value.x, min.x, max.x), std::clamp(value.y, min.y, max.y), std::clamp(value.z, min.z, max.z));
}
inline Vector4 Clamp(const Vector4 &value, const Vector4 &min, const Vector4 &max) {
    return Vector4(std::clamp(value.x, min.x, max.x), std::clamp(value.y, min.y, max.y), std::clamp(value.z, min.z, max.z), std::clamp(value.w, min.w, max.w));
}