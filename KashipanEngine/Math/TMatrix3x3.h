#pragma once
#include <type_traits>

namespace KashipanEngine {

template<typename T>
struct TVector2;

template<typename T>
struct TMatrix3x3 final {
    static_assert(std::is_arithmetic_v<T>, "T must be an arithmetic type");

    TMatrix3x3() noexcept = default;
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

    TMatrix3x3<T> &operator+=(const TMatrix3x3<T> &matrix) noexcept;
    TMatrix3x3<T> &operator-=(const TMatrix3x3<T> &matrix) noexcept;
    TMatrix3x3<T> &operator*=(T scalar) noexcept;
    TMatrix3x3<T> &operator*=(const TMatrix3x3<T> &matrix) noexcept;
    constexpr TMatrix3x3<T> operator+(const TMatrix3x3<T> &matrix) const noexcept;
    constexpr TMatrix3x3<T> operator-(const TMatrix3x3<T> &matrix) const noexcept;
    constexpr TMatrix3x3<T> operator*(T scalar) const noexcept;
    constexpr TMatrix3x3<T> operator*(const TMatrix3x3<T> &matrix) const noexcept;

    [[nodiscard]] static const TMatrix3x3<T> Identity() noexcept;
    [[nodiscard]] const TMatrix3x3<T> Transpose() const noexcept;
    [[nodiscard]] const T Determinant() const noexcept;
    [[nodiscard]] TMatrix3x3<T> Inverse() const;

    void MakeIdentity() noexcept;
    void MakeTranspose() noexcept;
    void MakeInverse() noexcept;

    void MakeTranslate(const TVector2<T> &translate) noexcept;
    void MakeScale(const TVector2<T> &scale) noexcept;
    void MakeRotate(T radian) noexcept;
    void MakeAffine(const TVector2<T> &scale, T radian, const TVector2<T> &translate) noexcept;

    T m[3][3];
};

// 型エイリアス
using Matrix3x3i = TMatrix3x3<int>;
using Matrix3x3f = TMatrix3x3<float>;
using Matrix3x3d = TMatrix3x3<double>;

} // namespace KashipanEngine