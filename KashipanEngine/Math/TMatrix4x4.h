#pragma once
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector3;

template<typename T>
struct TMatrix4x4 final {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

    // 大量に呼び出されるであろうデフォルトコンストラクタは軽量化のため何もしないようにしておく
    TMatrix4x4() noexcept = default;
    constexpr TMatrix4x4(
        T m00, T m01, T m02, T m03,
        T m10, T m11, T m12, T m13,
        T m20, T m21, T m22, T m23,
        T m30, T m31, T m32, T m33) noexcept
        : m{
            {m00, m01, m02, m03},
            {m10, m11, m12, m13},
            {m20, m21, m22, m23},
            {m30, m31, m32, m33}
        } {
    }

    // 行列式計算用の2x2行列
    struct TMatrix2x2 {
        constexpr TMatrix2x2(T m00, T m01, T m10, T m11) noexcept
            : m{
                {m00, m01},
                {m10, m11}
            } {
        }
        /// @brief 行列式を計算する
        /// @return 行列式
        [[nodiscard]] constexpr T Determinant() const noexcept {
            return m[0][0] * m[1][1] - m[0][1] * m[1][0];
        }
    private:
        T m[2][2];
    };

    // 行列式計算用の3x3行列
    struct TMatrix3x3 {
        constexpr TMatrix3x3(
            T m00, T m01, T m02,
            T m10, T m11, T m12,
            T m20, T m21, T m22) noexcept
            : m{
                {m00, m01, m02},
                {m10, m11, m12},
                {m20, m21, m22}
            } {
        }
        /// @brief 行列式を計算する
        /// @return 行列式
        [[nodiscard]] constexpr T Determinant() const noexcept;
    private:
        T m[3][3];
    };

    TMatrix4x4<T> &operator+=(const TMatrix4x4<T> &matrix) noexcept;
    TMatrix4x4<T> &operator-=(const TMatrix4x4<T> &matrix) noexcept;
    TMatrix4x4<T> &operator*=(const T scalar) noexcept;
    TMatrix4x4<T> &operator*=(const TMatrix4x4<T> &matrix) noexcept;
    constexpr const TMatrix4x4<T> operator+(const TMatrix4x4<T> &matrix) const noexcept;
    constexpr const TMatrix4x4<T> operator-(const TMatrix4x4<T> &matrix) const noexcept;
    constexpr const TMatrix4x4<T> operator*(const T scalar) const noexcept;
    constexpr const TMatrix4x4<T> operator*(const TMatrix4x4<T> &matrix) const noexcept;

    /// @brief 単位行列を取得する
    /// @return 単位行列
    [[nodiscard]] static const TMatrix4x4<T> Identity() noexcept;

    /// @brief 転置行列を取得する
    /// @return 転置行列
    [[nodiscard]] const TMatrix4x4<T> Transpose() noexcept;

    /// @brief 行列式を計算する
    /// @return 行列式
    [[nodiscard]] const T Determinant() const noexcept;

    /// @brief 逆行列を計算する
    /// @return 逆行列
    [[nodiscard]] TMatrix4x4<T> Inverse() const;

    /// @brief 自身を単位行列にする
    void MakeIdentity() noexcept;

    /// @brief 自身を転置行列にする
    void MakeTranspose() noexcept;

    /// @brief 自身を逆行列にする
    void MakeInverse() noexcept;

    /// @brief 平行移動行列を生成する
    /// @param translate 平行移動ベクトル
    /// @return 平行移動行列
    void MakeTranslate(const TVector3<T> &translate) noexcept;

    /// @brief 拡大縮小行列を生成する
    /// @param scale 拡大縮小ベクトル
    /// @return 拡大縮小行列
    void MakeScale(const TVector3<T> &scale) noexcept;

    /// @brief 回転行列を生成する
    /// @param rotate 回転角度
    /// @return 回転行列
    void MakeRotate(const TVector3<T> &rotate) noexcept;

    /// @brief 回転行列を生成する
    /// @param radianX 回転角度 X
    /// @param radianY 回転角度 Y
    /// @param radianZ 回転角度 Z
    /// @return 回転行列
    void MakeRotate(
        const T radianX,
        const T radianY,
        const T radianZ) noexcept;

    /// @brief X軸回転行列を生成する
    /// @param radian 回転角度
    /// @return X軸回転行列
    void MakeRotateX(const T radian) noexcept;

    /// @brief Y軸回転行列を生成する
    /// @param radian 回転角度
    /// @return Y軸回転行列
    void MakeRotateY(const T radian) noexcept;

    /// @brief Z軸回転行列を生成する
    /// @param radian 回転角度
    /// @return Z軸回転行列
    void MakeRotateZ(const T radian) noexcept;

    /// @brief アフィン行列を生成する
    /// @param scale 拡大縮小ベクトル
    /// @param rotate 回転角度
    /// @param translate 平行移動ベクトル
    void MakeAffine(
        const TVector3<T> &scale,
        const TVector3<T> &rotate,
        const TVector3<T> &translate) noexcept;

    /// @brief ビュー行列を生成する
    /// @param eyePos カメラの位置
    /// @param targetPos 注視点
    /// @param upVector 上方向ベクトル
    void MakeViewMatrix(const TVector3<T> &eyePos, const TVector3<T> &targetPos, const TVector3<T> &upVector) noexcept;

    /// @brief 透視投影行列を生成する
    /// @param fovY 画角 Y
    /// @param aspectRatio アスペクト比
    /// @param nearClip 近平面への距離
    /// @param farClip 遠平面への距離
    void MakePerspectiveFovMatrix(const T fovY, const T aspectRatio, const T nearClip, const T farClip) noexcept;

    /// @brief 正射影行列を生成する
    /// @param left 左端
    /// @param top 上端
    /// @param right 右端
    /// @param bottom 下端
    /// @param nearClip 近平面への距離
    /// @param farClip 遠平面への距離
    void MakeOrthographicMatrix(const T left, const T top, const T right, const T bottom, const T nearClip, const T farClip) noexcept;

    /// @brief ビューポート行列を生成する
    /// @param left 左端
    /// @param top 上端
    /// @param width 幅
    /// @param height 高さ
    /// @param minDepth 最小深度
    /// @param maxDepth 最大深度
    void MakeViewportMatrix(const T left, const T top, const T width, const T height, const T minDepth, const T maxDepth) noexcept;

    T m[4][4];
};

// 型エイリアス
using Matrix4x4i = TMatrix4x4<int>;
using Matrix4x4f = TMatrix4x4<float>;
using Matrix4x4d = TMatrix4x4<double>;

} // namespace KashipanEngine