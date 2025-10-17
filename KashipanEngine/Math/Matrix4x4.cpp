#include "Matrix4x4.h"
#include "Math/Vector3.h"
#include "Utilities/MathUtils.h"

namespace KashipanEngine {

constexpr float Matrix4x4::Matrix2x2::Determinant() const noexcept {
    return m[0][0] * m[1][1] - m[0][1] * m[1][0];
}

constexpr float Matrix4x4::Matrix3x3::Determinant() const noexcept {
    float c00 = Matrix2x2(m[1][1], m[1][2], m[2][1], m[2][2]).Determinant();
    float c01 = -(Matrix2x2(m[1][0], m[1][2], m[2][0], m[2][2]).Determinant());
    float c02 = Matrix2x2(m[1][0], m[1][1], m[2][0], m[2][1]).Determinant();
    return m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
}

Matrix4x4 &Matrix4x4::operator+=(const Matrix4x4 &matrix) noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    m[0][0] += matrix.m[0][0];
    m[0][1] += matrix.m[0][1];
    m[0][2] += matrix.m[0][2];
    m[0][3] += matrix.m[0][3];

    m[1][0] += matrix.m[1][0];
    m[1][1] += matrix.m[1][1];
    m[1][2] += matrix.m[1][2];
    m[1][3] += matrix.m[1][3];

    m[2][0] += matrix.m[2][0];
    m[2][1] += matrix.m[2][1];
    m[2][2] += matrix.m[2][2];
    m[2][3] += matrix.m[2][3];

    m[3][0] += matrix.m[3][0];
    m[3][1] += matrix.m[3][1];
    m[3][2] += matrix.m[3][2];
    m[3][3] += matrix.m[3][3];

    return *this;
}

Matrix4x4 &Matrix4x4::operator-=(const Matrix4x4 &matrix) noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    m[0][0] -= matrix.m[0][0];
    m[0][1] -= matrix.m[0][1];
    m[0][2] -= matrix.m[0][2];
    m[0][3] -= matrix.m[0][3];

    m[1][0] -= matrix.m[1][0];
    m[1][1] -= matrix.m[1][1];
    m[1][2] -= matrix.m[1][2];
    m[1][3] -= matrix.m[1][3];

    m[2][0] -= matrix.m[2][0];
    m[2][1] -= matrix.m[2][1];
    m[2][2] -= matrix.m[2][2];
    m[2][3] -= matrix.m[2][3];

    m[3][0] -= matrix.m[3][0];
    m[3][1] -= matrix.m[3][1];
    m[3][2] -= matrix.m[3][2];
    m[3][3] -= matrix.m[3][3];

    return *this;
}

Matrix4x4 &Matrix4x4::operator*=(const float scalar) noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    m[0][0] *= scalar;
    m[0][1] *= scalar;
    m[0][2] *= scalar;
    m[0][3] *= scalar;

    m[1][0] *= scalar;
    m[1][1] *= scalar;
    m[1][2] *= scalar;
    m[1][3] *= scalar;

    m[2][0] *= scalar;
    m[2][1] *= scalar;
    m[2][2] *= scalar;
    m[2][3] *= scalar;

    m[3][0] *= scalar;
    m[3][1] *= scalar;
    m[3][2] *= scalar;
    m[3][3] *= scalar;

    return *this;
}

Matrix4x4 &Matrix4x4::operator*=(const Matrix4x4 &matrix) noexcept {
    *this = *this * matrix;
    return *this;
}

constexpr const Matrix4x4 Matrix4x4::operator+(const Matrix4x4 &matrix) const noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    return Matrix4x4(
        m[0][0] + matrix.m[0][0], m[0][1] + matrix.m[0][1], m[0][2] + matrix.m[0][2], m[0][3] + matrix.m[0][3],
        m[1][0] + matrix.m[1][0], m[1][1] + matrix.m[1][1], m[1][2] + matrix.m[1][2], m[1][3] + matrix.m[1][3],
        m[2][0] + matrix.m[2][0], m[2][1] + matrix.m[2][1], m[2][2] + matrix.m[2][2], m[2][3] + matrix.m[2][3],
        m[3][0] + matrix.m[3][0], m[3][1] + matrix.m[3][1], m[3][2] + matrix.m[3][2], m[3][3] + matrix.m[3][3]
    );
}

constexpr const Matrix4x4 Matrix4x4::operator-(const Matrix4x4 &matrix) const noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    return Matrix4x4(
        m[0][0] - matrix.m[0][0], m[0][1] - matrix.m[0][1], m[0][2] - matrix.m[0][2], m[0][3] - matrix.m[0][3],
        m[1][0] - matrix.m[1][0], m[1][1] - matrix.m[1][1], m[1][2] - matrix.m[1][2], m[1][3] - matrix.m[1][3],
        m[2][0] - matrix.m[2][0], m[2][1] - matrix.m[2][1], m[2][2] - matrix.m[2][2], m[2][3] - matrix.m[2][3],
        m[3][0] - matrix.m[3][0], m[3][1] - matrix.m[3][1], m[3][2] - matrix.m[3][2], m[3][3] - matrix.m[3][3]
    );
}

constexpr const Matrix4x4 Matrix4x4::operator*(const float scalar) const noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    return Matrix4x4(
        m[0][0] * scalar, m[0][1] * scalar, m[0][2] * scalar, m[0][3] * scalar,
        m[1][0] * scalar, m[1][1] * scalar, m[1][2] * scalar, m[1][3] * scalar,
        m[2][0] * scalar, m[2][1] * scalar, m[2][2] * scalar, m[2][3] * scalar,
        m[3][0] * scalar, m[3][1] * scalar, m[3][2] * scalar, m[3][3] * scalar
    );
}

constexpr const Matrix4x4 Matrix4x4::operator*(const Matrix4x4 &matrix) const noexcept {
    // 少しでも速度を稼ぐためにループではなく展開する
    return Matrix4x4(
        m[0][0] * matrix.m[0][0] + m[0][1] * matrix.m[1][0] + m[0][2] * matrix.m[2][0] + m[0][3] * matrix.m[3][0],
        m[0][0] * matrix.m[0][1] + m[0][1] * matrix.m[1][1] + m[0][2] * matrix.m[2][1] + m[0][3] * matrix.m[3][1],
        m[0][0] * matrix.m[0][2] + m[0][1] * matrix.m[1][2] + m[0][2] * matrix.m[2][2] + m[0][3] * matrix.m[3][2],
        m[0][0] * matrix.m[0][3] + m[0][1] * matrix.m[1][3] + m[0][2] * matrix.m[2][3] + m[0][3] * matrix.m[3][3],
        m[1][0] * matrix.m[0][0] + m[1][1] * matrix.m[1][0] + m[1][2] * matrix.m[2][0] + m[1][3] * matrix.m[3][0],
        m[1][0] * matrix.m[0][1] + m[1][1] * matrix.m[1][1] + m[1][2] * matrix.m[2][1] + m[1][3] * matrix.m[3][1],
        m[1][0] * matrix.m[0][2] + m[1][1] * matrix.m[1][2] + m[1][2] * matrix.m[2][2] + m[1][3] * matrix.m[3][2],
        m[1][0] * matrix.m[0][3] + m[1][1] * matrix.m[1][3] + m[1][2] * matrix.m[2][3] + m[1][3] * matrix.m[3][3],
        m[2][0] * matrix.m[0][0] + m[2][1] * matrix.m[1][0] + m[2][2] * matrix.m[2][0] + m[2][3] * matrix.m[3][0],
        m[2][0] * matrix.m[0][1] + m[2][1] * matrix.m[1][1] + m[2][2] * matrix.m[2][1] + m[2][3] * matrix.m[3][1],
        m[2][0] * matrix.m[0][2] + m[2][1] * matrix.m[1][2] + m[2][2] * matrix.m[2][2] + m[2][3] * matrix.m[3][2],
        m[2][0] * matrix.m[0][3] + m[2][1] * matrix.m[1][3] + m[2][2] * matrix.m[2][3] + m[2][3] * matrix.m[3][3],
        m[3][0] * matrix.m[0][0] + m[3][1] * matrix.m[1][0] + m[3][2] * matrix.m[2][0] + m[3][3] * matrix.m[3][0],
        m[3][0] * matrix.m[0][1] + m[3][1] * matrix.m[1][1] + m[3][2] * matrix.m[2][1] + m[3][3] * matrix.m[3][1],
        m[3][0] * matrix.m[0][2] + m[3][1] * matrix.m[1][2] + m[3][2] * matrix.m[2][2] + m[3][3] * matrix.m[3][2],
        m[3][0] * matrix.m[0][3] + m[3][1] * matrix.m[1][3] + m[3][2] * matrix.m[2][3] + m[3][3] * matrix.m[3][3]
    );
}

const Matrix4x4 Matrix4x4::Identity() noexcept {
    return MathUtils::Matrix4x4Identity();
}

const Matrix4x4 Matrix4x4::Transpose() noexcept {
    return MathUtils::Matrix4x4Transpose(*this);
}

const float Matrix4x4::Determinant() const noexcept {
    return MathUtils::Matrix4x4Determinant(*this);
}

Matrix4x4 Matrix4x4::Inverse() const {
    return MathUtils::Matrix4x4Inverse(*this);
}

void Matrix4x4::MakeIdentity() noexcept {
    *this = MathUtils::Matrix4x4Identity();
}

void Matrix4x4::MakeTranspose() noexcept {
    *this = MathUtils::Matrix4x4Transpose(*this);
}

void Matrix4x4::MakeInverse() noexcept {
    *this = MathUtils::Matrix4x4Inverse(*this);
}

void Matrix4x4::MakeTranslate(const Vector3 &translate) noexcept {
    *this = MathUtils::Matrix4x4MakeTranslate(translate);
}

void Matrix4x4::MakeScale(const Vector3 &scale) noexcept {
    *this = MathUtils::Matrix4x4MakeScale(scale);
}

void Matrix4x4::MakeRotate(const Vector3 &rotate) noexcept {
    *this = MathUtils::Matrix4x4MakeRotate(rotate);
}

void Matrix4x4::MakeRotate(
    const float radianX,
    const float radianY,
    const float radianZ) noexcept {
    *this = MathUtils::Matrix4x4MakeRotate(radianX, radianY, radianZ);
}

void Matrix4x4::MakeRotateX(const float radian) noexcept {
    *this = MathUtils::Matrix4x4MakeRotateX(radian);
}

void Matrix4x4::MakeRotateY(const float radian) noexcept {
    *this = MathUtils::Matrix4x4MakeRotateY(radian);
}

void Matrix4x4::MakeRotateZ(const float radian) noexcept {
    *this = MathUtils::Matrix4x4MakeRotateZ(radian);
}

void Matrix4x4::MakeAffine(
    const Vector3 &scale,
    const Vector3 &rotate,
    const Vector3 &translate) noexcept {
    *this = MathUtils::Matrix4x4MakeAffine(scale, rotate, translate);
}

void Matrix4x4::MakeViewMatrix(const Vector3 &eyePos, const Vector3 &targetPos, const Vector3 &upVector) noexcept {
    *this = MathUtils::Matrix4x4MakeViewMatrix(eyePos, targetPos, upVector);
}

void Matrix4x4::MakePerspectiveFovMatrix(const float fovY, const float aspectRatio, const float nearClip, const float farClip) noexcept {
    *this = MathUtils::Matrix4x4MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip);
}

void Matrix4x4::MakeOrthographicMatrix(const float left, const float top, const float right, const float bottom, const float nearClip, const float farClip) noexcept {
    *this = MathUtils::Matrix4x4MakeOrthographicMatrix(left, top, right, bottom, nearClip, farClip);
}

void Matrix4x4::MakeViewportMatrix(const float left, const float top, const float width, const float height, const float minDepth, const float maxDepth) noexcept {
    *this = MathUtils::Matrix4x4MakeViewportMatrix(left, top, width, height, minDepth, maxDepth);
}

} // namespace KashipanEngine