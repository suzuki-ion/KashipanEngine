#pragma once
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