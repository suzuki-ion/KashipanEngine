#pragma once

struct Matrix4x4;
struct Vector3;

namespace KashipanEngine {

namespace MathUtils {

/// @brief Matrix4x4の単位行列取得
/// @return 単位行列
Matrix4x4 Matrix4x4Identity() noexcept;

/// @brief Matrix4x4の転置行列取得
/// @param matrix 行列
/// @return 転置行列
Matrix4x4 Matrix4x4Transpose(const Matrix4x4 &matrix) noexcept;

/// @brief Matrix4x4の行列式計算
/// @param matrix 行列
/// @return 行列式
float Matrix4x4Determinant(const Matrix4x4 &matrix) noexcept;

/// @brief Matrix4x4の逆行列計算
/// @param matrix 行列
/// @return 逆行列
Matrix4x4 Matrix4x4Inverse(const Matrix4x4 &matrix);

/// @brief Matrix4x4の平行移動行列生成
/// @param translate 平行移動ベクトル
/// @return 平行移動行列
Matrix4x4 Matrix4x4MakeTranslate(const Vector3 &translate) noexcept;

/// @brief Matrix4x4の拡大縮小行列生成
/// @param scale 拡大縮小ベクトル
/// @return 拡大縮小行列
Matrix4x4 Matrix4x4MakeScale(const Vector3 &scale) noexcept;

/// @brief Matrix4x4の回転行列生成
/// @param rotate 回転角度
/// @return 回転行列
Matrix4x4 Matrix4x4MakeRotate(const Vector3 &rotate) noexcept;

/// @brief Matrix4x4の回転行列生成
/// @param radianX X軸回転角度
/// @param radianY Y軸回転角度
/// @param radianZ Z軸回転角度
/// @return 回転行列
Matrix4x4 Matrix4x4MakeRotate(const float radianX, const float radianY, const float radianZ) noexcept;

/// @brief Matrix4x4のX軸回転行列生成
/// @param radian 回転角度
/// @return X軸回転行列
Matrix4x4 Matrix4x4MakeRotateX(const float radian) noexcept;

/// @brief Matrix4x4のY軸回転行列生成
/// @param radian 回転角度
/// @return Y軸回転行列
Matrix4x4 Matrix4x4MakeRotateY(const float radian) noexcept;

/// @brief Matrix4x4のZ軸回転行列生成
/// @param radian 回転角度
/// @return Z軸回転行列
Matrix4x4 Matrix4x4MakeRotateZ(const float radian) noexcept;

/// @brief Matrix4x4のアフィン行列生成
/// @param scale 拡大縮小ベクトル
/// @param rotate 回転角度
/// @param translate 平行移動ベクトル
/// @return アフィン行列
Matrix4x4 Matrix4x4MakeAffine(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) noexcept;

/// @brief Matrix4x4のビュー行列生成
/// @param eyePos カメラの位置
/// @param targetPos 注視点
/// @param upVector 上方向ベクトル
/// @return ビュー行列
Matrix4x4 Matrix4x4MakeViewMatrix(const Vector3 &eyePos, const Vector3 &targetPos, const Vector3 &upVector) noexcept;

/// @brief Matrix4x4の透視投影行列生成
/// @param fovY 画角 Y
/// @param aspectRatio アスペクト比
/// @param nearClip 近平面への距離
/// @param farClip 遠平面への距離
/// @return 透視投影行列
Matrix4x4 Matrix4x4MakePerspectiveFovMatrix(const float fovY, const float aspectRatio, const float nearClip, const float farClip) noexcept;

/// @brief Matrix4x4の正射影行列生成
/// @param left 左端
/// @param top 上端
/// @param right 右端
/// @param bottom 下端
/// @param nearClip 近平面への距離
/// @param farClip 遠平面への距離
/// @return 正射影行列
Matrix4x4 Matrix4x4MakeOrthographicMatrix(const float left, const float top, const float right, const float bottom, const float nearClip, const float farClip) noexcept;

/// @brief Matrix4x4のビューポート行列生成
/// @param left 左端
/// @param top 上端
/// @param width 幅
/// @param height 高さ
/// @param minDepth 最小深度
/// @param maxDepth 最大深度
/// @return ビューポート行列
Matrix4x4 Matrix4x4MakeViewportMatrix(const float left, const float top, const float width, const float height, const float minDepth, const float maxDepth) noexcept;

} // namespace MathUtils
} // namespace KashipanEngine
