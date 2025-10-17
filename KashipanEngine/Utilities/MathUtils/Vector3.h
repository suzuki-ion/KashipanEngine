#pragma once
#include <vector>

namespace KashipanEngine {

struct Vector3;
struct Matrix4x4;

namespace MathUtils {

/// @brief Vector3の線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector3 Lerp(const Vector3 &start, const Vector3 &end, float t) noexcept;

/// @brief Vector3の球面線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector3 Slerp(const Vector3 &start, const Vector3 &end, float t) noexcept;

/// @brief Vector3のベジェ曲線補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector3 Bezier(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, float t) noexcept;

/// @brief Vector3のCatmull-Rom補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param p3 制御点3
/// @param t 補間パラメータ
/// @return 補間されたベクトル
Vector3 CatmullRomInterpolation(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, float t) noexcept;

/// @brief Vector3のCatmull-Rom位置取得
/// @param points 制御点リスト
/// @param t 補間パラメータ
/// @param isLoop ループするかどうか
/// @return 補間されたベクトル
Vector3 CatmullRomPosition(const std::vector<Vector3> &points, float t, bool isLoop = false);

/// @brief Vector3の内積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 内積値
constexpr float Dot(const Vector3 &vector1, const Vector3 &vector2) noexcept;

/// @brief Vector3の外積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 外積ベクトル
Vector3 Cross(const Vector3 &vector1, const Vector3 &vector2) noexcept;

/// @brief Vector3の長さ計算
/// @param vector ベクトル
/// @return 長さ
float Length(const Vector3 &vector) noexcept;

/// @brief Vector3の長さの二乗計算
/// @param vector ベクトル
/// @return 長さの二乗
constexpr float LengthSquared(const Vector3 &vector) noexcept;

/// @brief Vector3の正規化
/// @param vector ベクトル
/// @return 正規化されたベクトル
Vector3 Normalize(const Vector3 &vector);

/// @brief Vector3の投影ベクトル計算
/// @param vector 投影されるベクトル
/// @param onto 投影先ベクトル
/// @return 投影ベクトル
Vector3 Projection(const Vector3 &vector, const Vector3 &onto) noexcept;

/// @brief Vector3の垂直ベクトル取得
/// @param vector ベクトル
/// @return 垂直ベクトル
Vector3 Perpendicular(const Vector3 &vector) noexcept;

/// @brief Vector3の拒絶ベクトル計算
/// @param vector ベクトル
/// @param onto 拒絶先ベクトル
/// @return 拒絶ベクトル
Vector3 Rejection(const Vector3 &vector, const Vector3 &onto) noexcept;

/// @brief Vector3の反射ベクトル計算
/// @param vector ベクトル
/// @param normal 法線ベクトル
/// @return 反射ベクトル
Vector3 Reflection(const Vector3 &vector, const Vector3 &normal) noexcept;

/// @brief Vector3間の距離計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 距離
float Distance(const Vector3 &vector1, const Vector3 &vector2);

/// @brief Vector3の行列変換
/// @param vector ベクトル
/// @param mat 変換行列
/// @return 変換されたベクトル
Vector3 Transform(const Vector3 &vector, const Matrix4x4 &mat) noexcept;

} // namespace MathUtils
} // namespace KashipanEngine
