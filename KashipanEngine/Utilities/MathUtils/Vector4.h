#pragma once

namespace KashipanEngine {

struct Vector4;

namespace MathUtils {

/// @brief Vector4の線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector4 Lerp(const Vector4 &start, const Vector4 &end, float t) noexcept;

} // namespace MathUtils
} // namespace KashipanEngine
