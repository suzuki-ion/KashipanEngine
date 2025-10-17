#include "Matrix4x4.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include <cmath>

namespace KashipanEngine {
namespace MathUtils {

Matrix4x4 Matrix4x4Identity() noexcept {
    return Matrix4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 Matrix4x4Transpose(const Matrix4x4 &matrix) noexcept {
    return Matrix4x4(
        matrix.m[0][0], matrix.m[1][0], matrix.m[2][0], matrix.m[3][0],
        matrix.m[0][1], matrix.m[1][1], matrix.m[2][1], matrix.m[3][1],
        matrix.m[0][2], matrix.m[1][2], matrix.m[2][2], matrix.m[3][2],
        matrix.m[0][3], matrix.m[1][3], matrix.m[2][3], matrix.m[3][3]
    );
}

float Matrix4x4Determinant(const Matrix4x4 &matrix) noexcept {
    float c00 = Matrix4x4::Matrix3x3(
        matrix.m[1][1], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][1], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][1], matrix.m[3][2], matrix.m[3][3]).Determinant();
    float c01 = -(Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][2], matrix.m[3][3]).Determinant());
    float c02 = Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][3]).Determinant();
    float c03 = -(Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][2],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][2],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]).Determinant());
    return (matrix.m[0][0] * c00) + (matrix.m[0][1] * c01) + (matrix.m[0][2] * c02) + (matrix.m[0][3] * c03);
}

Matrix4x4 Matrix4x4Inverse(const Matrix4x4 &matrix) {
    float c00 = Matrix4x4::Matrix3x3(
        matrix.m[1][1], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][1], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][1], matrix.m[3][2], matrix.m[3][3]).Determinant();
    float c01 = -(Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][2], matrix.m[3][3]).Determinant());
    float c02 = Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][3]).Determinant();
    float c03 = -(Matrix4x4::Matrix3x3(
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][2],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][2],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]).Determinant());

    float c10 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][1], matrix.m[0][2], matrix.m[0][3],
        matrix.m[2][1], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][1], matrix.m[3][2], matrix.m[3][3]).Determinant());
    float c11 = Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][2], matrix.m[0][3],
        matrix.m[2][0], matrix.m[2][2], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][2], matrix.m[3][3]).Determinant();
    float c12 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][3],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][3],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][3]).Determinant());
    float c13 = Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][2],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][2],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]).Determinant();

    float c20 = Matrix4x4::Matrix3x3(
        matrix.m[0][1], matrix.m[0][2], matrix.m[0][3],
        matrix.m[1][1], matrix.m[1][2], matrix.m[1][3],
        matrix.m[3][1], matrix.m[3][2], matrix.m[3][3]).Determinant();
    float c21 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][2], matrix.m[0][3],
        matrix.m[1][0], matrix.m[1][2], matrix.m[1][3],
        matrix.m[3][0], matrix.m[3][2], matrix.m[3][3]).Determinant());
    float c22 = Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][3],
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][3],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][3]).Determinant();
    float c23 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][2],
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][2],
        matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]).Determinant());

    float c30 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][1], matrix.m[0][2], matrix.m[0][3],
        matrix.m[1][1], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][1], matrix.m[2][2], matrix.m[2][3]).Determinant());
    float c31 = Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][2], matrix.m[0][3],
        matrix.m[1][0], matrix.m[1][2], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][2], matrix.m[2][3]).Determinant();
    float c32 = -(Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][3],
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][3],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][3]).Determinant());
    float c33 = Matrix4x4::Matrix3x3(
        matrix.m[0][0], matrix.m[0][1], matrix.m[0][2],
        matrix.m[1][0], matrix.m[1][1], matrix.m[1][2],
        matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]).Determinant();

    float det = 1.0f / (matrix.m[0][0] * c00 + matrix.m[0][1] * c01 + matrix.m[0][2] * c02 + matrix.m[0][3] * c03);

    return Matrix4x4(
        c00 * det, c10 * det, c20 * det, c30 * det,
        c01 * det, c11 * det, c21 * det, c31 * det,
        c02 * det, c12 * det, c22 * det, c32 * det,
        c03 * det, c13 * det, c23 * det, c33 * det
    );
}

Matrix4x4 Matrix4x4MakeTranslate(const Vector3 &translate) noexcept {
    return Matrix4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f
    );
}

Matrix4x4 Matrix4x4MakeScale(const Vector3 &scale) noexcept {
    return Matrix4x4(
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 Matrix4x4MakeRotate(const Vector3 &rotate) noexcept {
    Matrix4x4 mX = Matrix4x4MakeRotateX(rotate.x);
    Matrix4x4 mY = Matrix4x4MakeRotateY(rotate.y);
    Matrix4x4 mZ = Matrix4x4MakeRotateZ(rotate.z);
    return mX * mY * mZ;
}

Matrix4x4 Matrix4x4MakeRotate(const float radianX, const float radianY, const float radianZ) noexcept {
    Matrix4x4 mX = Matrix4x4MakeRotateX(radianX);
    Matrix4x4 mY = Matrix4x4MakeRotateY(radianY);
    Matrix4x4 mZ = Matrix4x4MakeRotateZ(radianZ);
    return mX * mY * mZ;
}

Matrix4x4 Matrix4x4MakeRotateX(const float radian) noexcept {
    return Matrix4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, std::cos(radian), std::sin(radian), 0.0f,
        0.0f, -std::sin(radian), std::cos(radian), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 Matrix4x4MakeRotateY(const float radian) noexcept {
    return Matrix4x4(
        std::cos(radian), 0.0f, -std::sin(radian), 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        std::sin(radian), 0.0f, std::cos(radian), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 Matrix4x4MakeRotateZ(const float radian) noexcept {
    return Matrix4x4(
        std::cos(radian), std::sin(radian), 0.0f, 0.0f,
        -std::sin(radian), std::cos(radian), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 Matrix4x4MakeAffine(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) noexcept {
    Matrix4x4 mX = Matrix4x4MakeRotateX(rotate.x);
    Matrix4x4 mY = Matrix4x4MakeRotateY(rotate.y);
    Matrix4x4 mZ = Matrix4x4MakeRotateZ(rotate.z);
    Matrix4x4 mT = Matrix4x4MakeTranslate(translate);
    Matrix4x4 mS = Matrix4x4MakeScale(scale);
    return mS * (mX * mY * mZ) * mT;
}

Matrix4x4 Matrix4x4MakeViewMatrix(const Vector3 &eyePos, const Vector3 &targetPos, const Vector3 &upVector) noexcept {
    Vector3 zAxis = (targetPos - eyePos).Normalize();
    Vector3 xAxis = upVector.Cross(zAxis).Normalize();
    Vector3 yAxis = zAxis.Cross(xAxis).Normalize();
    return Matrix4x4(
        xAxis.x,            yAxis.x,            zAxis.x,            0.0f,
        xAxis.y,            yAxis.y,            zAxis.y,            0.0f,
        xAxis.z,            yAxis.z,            zAxis.z,            0.0f,
        -xAxis.Dot(eyePos), -yAxis.Dot(eyePos), -zAxis.Dot(eyePos), 1.0f
    );
}

Matrix4x4 Matrix4x4MakePerspectiveFovMatrix(const float fovY, const float aspectRatio, const float nearClip, const float farClip) noexcept {
    const float cot = 1.0f / std::tan(fovY / 2.0f);
    return Matrix4x4(
        cot / aspectRatio,  0.0f,   0.0f,                                           0.0f,
        0.0f,               cot,    0.0f,                                           0.0f,
        0.0f,               0.0f,   farClip / (farClip - nearClip),                 1.0f,
        0.0f,               0.0f,   -(nearClip * farClip) / (farClip - nearClip),   0.0f
    );
}

Matrix4x4 Matrix4x4MakeOrthographicMatrix(const float left, const float top, const float right, const float bottom, const float nearClip, const float farClip) noexcept {
    return Matrix4x4(
        2.0f / (right - left), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f / (farClip - nearClip), 0.0f,
        (left + right) / (left - right), (top + bottom) / (bottom - top), nearClip / (nearClip - farClip), 1.0f
    );
}

Matrix4x4 Matrix4x4MakeViewportMatrix(const float left, const float top, const float width, const float height, const float minDepth, const float maxDepth) noexcept {
    return Matrix4x4(
        width / 2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, -height / 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, maxDepth - minDepth, 0.0f,
        left + (width / 2.0f), top + (height / 2.0f), minDepth, 1.0f
    );
}

} // namespace MathUtils
} // namespace KashipanEngine