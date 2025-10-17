#pragma once
#include <vector>

namespace KashipanEngine {

struct Vector2;

namespace MathUtils {

/// @brief Vector2の線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector2 Lerp(const Vector2 &start, const Vector2 &end, float t) noexcept;

/// @brief Vector2の球面線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector2 Slerp(const Vector2 &start, const Vector2 &end, float t) noexcept;

/// @brief Vector2のベジェ曲線補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector2 Bezier(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, float t) noexcept;

/// @brief Vector2のCatmull-Rom補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param p3 制御点3
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector2 CatmullRomInterpolation(const Vector2 &p0, const Vector2 &p1, const Vector2 &p2, const Vector2 &p3, float t) noexcept;

/// @brief Vector2のCatmull-Rom位置取得
/// @param points 制御点リスト
/// @param t 補間パラメータ
/// @param isLoop ループするかどうか
/// @return 補間されたベクトル
Vector2 CatmullRomPosition(const std::vector<Vector2> &points, float t, bool isLoop = false);

/// @brief Vector2の内積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 内積値
constexpr float Dot(const Vector2 &vector1, const Vector2 &vector2) noexcept;

/// @brief Vector2の外積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 外積値
constexpr float Cross(const Vector2 &vector1, const Vector2 &vector2) noexcept;

/// @brief Vector2の長さ計算
/// @param vector ベクトル
/// @return 長さ
float Length(const Vector2 &vector) noexcept;

/// @brief Vector2の長さの二乗計算
/// @param vector ベクトル
/// @return 長さの二乗
constexpr float LengthSquared(const Vector2 &vector) noexcept;

/// @brief Vector2の正規化
/// @param vector ベクトル
/// @return 正規化されたベクトル
Vector2 Normalize(const Vector2 &vector);

/// @brief Vector2の投影ベクトル計算
/// @param vector 投影されるベクトル
/// @param onto 投影先ベクトル
/// @return 投影ベクトル
Vector2 Projection(const Vector2 &vector, const Vector2 &onto) noexcept;

/// @brief Vector2の垂直ベクトル取得
/// @param vector ベクトル
/// @return 垂直ベクトル
Vector2 Perpendicular(const Vector2 &vector) noexcept;

/// @brief Vector2の拒絶ベクトル計算
/// @param vector ベクトル
/// @param onto 拒絶先ベクトル
/// @return 拒絶ベクトル
Vector2 Rejection(const Vector2 &vector, const Vector2 &onto) noexcept;

/// @brief Vector2の反射ベクトル計算
/// @param vector ベクトル
/// @param normal 法線ベクトル
/// @return 反射ベクトル
Vector2 Reflection(const Vector2 &vector, const Vector2 &normal) noexcept;

/// @brief Vector2間の距離計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 距離
float Distance(const Vector2 &vector1, const Vector2 &vector2) noexcept;

} // namespace MathUtils
} // namespace KashipanEngine
