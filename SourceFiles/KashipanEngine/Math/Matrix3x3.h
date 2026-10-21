#pragma once

struct Vector2;

struct Matrix3x3 final {
    Matrix3x3() noexcept = default;
    constexpr Matrix3x3(
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22) noexcept
        : m{
            {m00, m01, m02},
            {m10, m11, m12},
            {m20, m21, m22}
        } {
    }

    // 行列式計算用の2x2行列
    struct Matrix2x2 {
        constexpr Matrix2x2(float m00, float m01, float m10, float m11) noexcept
            : m{
                {m00, m01},
                {m10, m11}
            } {
        }
        /// @brief 行列式を計算する
        /// @return 行列式
        [[nodiscard]] float Determinant() const noexcept;
    private:
        float m[2][2];
    };
    
    Matrix3x3 &operator+=(const Matrix3x3 &matrix) noexcept;
    Matrix3x3 &operator-=(const Matrix3x3 &matrix) noexcept;
    Matrix3x3 &operator*=(float scalar) noexcept;
    Matrix3x3 &operator*=(const Matrix3x3 &matrix) noexcept;
    constexpr Matrix3x3 operator+(const Matrix3x3 &matrix) const noexcept;
    constexpr Matrix3x3 operator-(const Matrix3x3 &matrix) const noexcept;
    constexpr Matrix3x3 operator*(float scalar) const noexcept;
    constexpr Matrix3x3 operator*(const Matrix3x3 &matrix) const noexcept;

    [[nodiscard]] static const Matrix3x3 Identity() noexcept;
    [[nodiscard]] const Matrix3x3 Transpose() const noexcept;
    [[nodiscard]] const float Determinant() const noexcept;
    [[nodiscard]] Matrix3x3 Inverse() const;

    void MakeIdentity() noexcept;
    void MakeTranspose() noexcept;
    void MakeInverse() noexcept;
    
    void MakeTranslate(const Vector2 &translate) noexcept;
    void MakeScale(const Vector2 &scale) noexcept;
    void MakeRotate(float radian) noexcept;
    void MakeAffine(const Vector2 &scale, float radian, const Vector2 &translate) noexcept;

    float m[3][3];
};

// constexpr関数は使用する全ての翻訳単位で定義が見えている必要があるため、ヘッダ内で定義する

inline constexpr Matrix3x3 Matrix3x3::operator+(const Matrix3x3 &matrix) const noexcept {
    return Matrix3x3(
        m[0][0] + matrix.m[0][0], m[0][1] + matrix.m[0][1], m[0][2] + matrix.m[0][2],
        m[1][0] + matrix.m[1][0], m[1][1] + matrix.m[1][1], m[1][2] + matrix.m[1][2],
        m[2][0] + matrix.m[2][0], m[2][1] + matrix.m[2][1], m[2][2] + matrix.m[2][2]
    );
}

inline constexpr Matrix3x3 Matrix3x3::operator-(const Matrix3x3 &matrix) const noexcept {
    return Matrix3x3(
        m[0][0] - matrix.m[0][0], m[0][1] - matrix.m[0][1], m[0][2] - matrix.m[0][2],
        m[1][0] - matrix.m[1][0], m[1][1] - matrix.m[1][1], m[1][2] - matrix.m[1][2],
        m[2][0] - matrix.m[2][0], m[2][1] - matrix.m[2][1], m[2][2] - matrix.m[2][2]
    );
}

inline constexpr Matrix3x3 Matrix3x3::operator*(float scalar) const noexcept {
    return Matrix3x3(
        m[0][0] * scalar, m[0][1] * scalar, m[0][2] * scalar,
        m[1][0] * scalar, m[1][1] * scalar, m[1][2] * scalar,
        m[2][0] * scalar, m[2][1] * scalar, m[2][2] * scalar
    );
}

inline constexpr Matrix3x3 Matrix3x3::operator*(const Matrix3x3 &matrix) const noexcept {
    return Matrix3x3(
        m[0][0] * matrix.m[0][0] + m[0][1] * matrix.m[1][0] + m[0][2] * matrix.m[2][0],
        m[0][0] * matrix.m[0][1] + m[0][1] * matrix.m[1][1] + m[0][2] * matrix.m[2][1],
        m[0][0] * matrix.m[0][2] + m[0][1] * matrix.m[1][2] + m[0][2] * matrix.m[2][2],

        m[1][0] * matrix.m[0][0] + m[1][1] * matrix.m[1][0] + m[1][2] * matrix.m[2][0],
        m[1][0] * matrix.m[0][1] + m[1][1] * matrix.m[1][1] + m[1][2] * matrix.m[2][1],
        m[1][0] * matrix.m[0][2] + m[1][1] * matrix.m[1][2] + m[1][2] * matrix.m[2][2],

        m[2][0] * matrix.m[0][0] + m[2][1] * matrix.m[1][0] + m[2][2] * matrix.m[2][0],
        m[2][0] * matrix.m[0][1] + m[2][1] * matrix.m[1][1] + m[2][2] * matrix.m[2][1],
        m[2][0] * matrix.m[0][2] + m[2][1] * matrix.m[1][2] + m[2][2] * matrix.m[2][2]
    );
}