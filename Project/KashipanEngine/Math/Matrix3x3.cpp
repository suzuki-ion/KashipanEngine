#include "Matrix3x3.h"
#include "Math/Vector2.h"
#include "Utilities/MathUtils.h"

float Matrix3x3::Matrix2x2::Determinant() const noexcept {
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

Matrix3x3 &Matrix3x3::operator+=(const Matrix3x3 &matrix) noexcept {
    m[0][0] += matrix.m[0][0];
    m[0][1] += matrix.m[0][1];
    m[0][2] += matrix.m[0][2];

    m[1][0] += matrix.m[1][0];
    m[1][1] += matrix.m[1][1];
    m[1][2] += matrix.m[1][2];

    m[2][0] += matrix.m[2][0];
    m[2][1] += matrix.m[2][1];
    m[2][2] += matrix.m[2][2];

    return *this;
}

Matrix3x3 &Matrix3x3::operator-=(const Matrix3x3 &matrix) noexcept {
    m[0][0] -= matrix.m[0][0];
    m[0][1] -= matrix.m[0][1];
    m[0][2] -= matrix.m[0][2];

    m[1][0] -= matrix.m[1][0];
    m[1][1] -= matrix.m[1][1];
    m[1][2] -= matrix.m[1][2];

    m[2][0] -= matrix.m[2][0];
    m[2][1] -= matrix.m[2][1];
    m[2][2] -= matrix.m[2][2];

    return *this;
}

Matrix3x3 &Matrix3x3::operator*=(const float scalar) noexcept {
    m[0][0] *= scalar;
    m[0][1] *= scalar;
    m[0][2] *= scalar;

    m[1][0] *= scalar;
    m[1][1] *= scalar;
    m[1][2] *= scalar;

    m[2][0] *= scalar;
    m[2][1] *= scalar;
    m[2][2] *= scalar;

    return *this;
}

Matrix3x3 &Matrix3x3::operator*=(const Matrix3x3 &matrix) noexcept {
    *this = *this * matrix;
    return *this;
}

constexpr Matrix3x3 Matrix3x3::operator+(const Matrix3x3 &matrix) const noexcept {
    return Matrix3x3(
        m[0][0] + matrix.m[0][0], m[0][1] + matrix.m[0][1], m[0][2] + matrix.m[0][2],
        m[1][0] + matrix.m[1][0], m[1][1] + matrix.m[1][1], m[1][2] + matrix.m[1][2],
        m[2][0] + matrix.m[2][0], m[2][1] + matrix.m[2][1], m[2][2] + matrix.m[2][2]
    );
}

constexpr Matrix3x3 Matrix3x3::operator-(const Matrix3x3 &matrix) const noexcept {
    return Matrix3x3(
        m[0][0] - matrix.m[0][0], m[0][1] - matrix.m[0][1], m[0][2] - matrix.m[0][2],
        m[1][0] - matrix.m[1][0], m[1][1] - matrix.m[1][1], m[1][2] - matrix.m[1][2],
        m[2][0] - matrix.m[2][0], m[2][1] - matrix.m[2][1], m[2][2] - matrix.m[2][2]
    );
}

constexpr Matrix3x3 Matrix3x3::operator*(float scalar) const noexcept {
    return Matrix3x3(
        m[0][0] * scalar, m[0][1] * scalar, m[0][2] * scalar,
        m[1][0] * scalar, m[1][1] * scalar, m[1][2] * scalar,
        m[2][0] * scalar, m[2][1] * scalar, m[2][2] * scalar
    );
}

constexpr Matrix3x3 Matrix3x3::operator*(const Matrix3x3 &matrix) const noexcept {
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

const Matrix3x3 Matrix3x3::Identity() noexcept {
    return KashipanEngine::MathUtils::Matrix3x3Identity();
}

const Matrix3x3 Matrix3x3::Transpose() const noexcept {
    return KashipanEngine::MathUtils::Matrix3x3Transpose(*this);
}

const float Matrix3x3::Determinant() const noexcept {
    return KashipanEngine::MathUtils::Matrix3x3Determinant(*this);
}

Matrix3x3 Matrix3x3::Inverse() const {
    return KashipanEngine::MathUtils::Matrix3x3Inverse(*this);
}

void Matrix3x3::MakeIdentity() noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3Identity();
}

void Matrix3x3::MakeTranspose() noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3Transpose(*this);
}

void Matrix3x3::MakeInverse() noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3Inverse(*this);
}

void Matrix3x3::MakeTranslate(const Vector2 &translate) noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3MakeTranslate(translate);
}

void Matrix3x3::MakeScale(const Vector2 &scale) noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3MakeScale(scale);
}

void Matrix3x3::MakeRotate(float radian) noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3MakeRotate(radian);
}

void Matrix3x3::MakeAffine(const Vector2 &scale, float radian, const Vector2 &translate) noexcept {
    *this = KashipanEngine::MathUtils::Matrix3x3MakeAffine(scale, radian, translate);
}