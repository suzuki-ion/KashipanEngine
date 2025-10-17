#pragma once

namespace KashipanEngine {

struct Matrix3x3;
struct Vector2;

namespace MathUtils {

/// @brief Matrix3x3の単位行列取得
/// @return 単位行列
Matrix3x3 Matrix3x3Identity() noexcept;

/// @brief Matrix3x3の転置行列取得
/// @param matrix 行列
/// @return 転置行列
Matrix3x3 Matrix3x3Transpose(const Matrix3x3 &matrix) noexcept;

/// @brief Matrix3x3の行列式計算
/// @param matrix 行列
/// @return 行列式
float Matrix3x3Determinant(const Matrix3x3 &matrix) noexcept;

/// @brief Matrix3x3の逆行列計算
/// @param matrix 行列
/// @return 逆行列
Matrix3x3 Matrix3x3Inverse(const Matrix3x3 &matrix);

/// @brief Matrix3x3の平行移動行列生成
/// @param translate 平行移動ベクトル
/// @return 平行移動行列
Matrix3x3 Matrix3x3MakeTranslate(const Vector2 &translate) noexcept;

/// @brief Matrix3x3の拡大縮小行列生成
/// @param scale 拡大縮小ベクトル
/// @return 拡大縮小行列
Matrix3x3 Matrix3x3MakeScale(const Vector2 &scale) noexcept;

/// @brief Matrix3x3の回転行列生成
/// @param radian 回転角度
/// @return 回転行列
Matrix3x3 Matrix3x3MakeRotate(float radian) noexcept;

/// @brief Matrix3x3のアフィン行列生成
/// @param scale 拡大縮小ベクトル
/// @param radian 回転角度
/// @param translate 平行移動ベクトル
/// @return アフィン行列
Matrix3x3 Matrix3x3MakeAffine(const Vector2 &scale, float radian, const Vector2 &translate) noexcept;

} // namespace MathUtils
} // namespace KashipanEngine
