#pragma once
#include <vector>
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector2;

namespace TMathUtils {

/// @brief TVector2の線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
template<typename T>
TVector2<T> Lerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept;

/// @brief TVector2の球面線形補間
/// @param start 開始ベクトル
/// @param end 終了ベクトル
/// @param t 補間パラメータ
/// @return 補間されたベクトル
template<typename T>
TVector2<T> Slerp(const TVector2<T> &start, const TVector2<T> &end, T t) noexcept;

/// @brief TVector2のベジェ曲線補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param t 補間パラメータ
/// @return 補間されたベクトル
template<typename T>
TVector2<T> Bezier(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, T t) noexcept;

/// @brief TVector2のCatmull-Rom補間
/// @param p0 制御点0
/// @param p1 制御点1
/// @param p2 制御点2
/// @param p3 制御点3
/// @param t 補間パラメータ
/// @return 補間されたベクトル
template<typename T>
TVector2<T> CatmullRomInterpolation(const TVector2<T> &p0, const TVector2<T> &p1, const TVector2<T> &p2, const TVector2<T> &p3, T t) noexcept;

/// @brief TVector2のCatmull-Rom位置取得
/// @param points 制御点リスト
/// @param t 補間パラメータ
/// @param isLoop ループするかどうか
/// @return 補間されたベクトル
template<typename T>
TVector2<T> CatmullRomPosition(const std::vector<TVector2<T>> &points, T t, bool isLoop = false);

/// @brief TVector2の内積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 内積値
template<typename T>
constexpr T Dot(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept;

/// @brief TVector2の外積計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 外積値
template<typename T>
constexpr T Cross(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept;

/// @brief TVector2の長さ計算
/// @param vector ベクトル
/// @return 長さ
template<typename T>
T Length(const TVector2<T> &vector) noexcept;

/// @brief TVector2の長さの二乗計算
/// @param vector ベクトル
/// @return 長さの二乗
template<typename T>
constexpr T LengthSquared(const TVector2<T> &vector) noexcept;

/// @brief TVector2の正規化
/// @param vector ベクトル
/// @return 正規化されたベクトル
template<typename T>
TVector2<T> Normalize(const TVector2<T> &vector);

/// @brief TVector2の投影ベクトル計算
/// @param vector 投影されるベクトル
/// @param onto 投影先ベクトル
/// @return 投影ベクトル
template<typename T>
TVector2<T> Projection(const TVector2<T> &vector, const TVector2<T> &onto) noexcept;

/// @brief TVector2の垂直ベクトル取得
/// @param vector ベクトル
/// @return 垂直ベクトル
template<typename T>
TVector2<T> Perpendicular(const TVector2<T> &vector) noexcept;

/// @brief TVector2の拒絶ベクトル計算
/// @param vector ベクトル
/// @param onto 拒絶先ベクトル
/// @return 拒絶ベクトル
template<typename T>
TVector2<T> Rejection(const TVector2<T> &vector, const TVector2<T> &onto) noexcept;

/// @brief TVector2の反射ベクトル計算
/// @param vector ベクトル
/// @param normal 法線ベクトル
/// @return 反射ベクトル
template<typename T>
TVector2<T> Reflection(const TVector2<T> &vector, const TVector2<T> &normal) noexcept;

/// @brief TVector2間の距離計算
/// @param vector1 ベクトル1
/// @param vector2 ベクトル2
/// @return 距離
template<typename T>
T Distance(const TVector2<T> &vector1, const TVector2<T> &vector2) noexcept;

} // namespace TMathUtils
} // namespace KashipanEngine