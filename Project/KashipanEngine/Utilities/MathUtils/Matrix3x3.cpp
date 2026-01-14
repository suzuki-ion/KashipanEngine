#include "Matrix3x3.h"
#include "Math/Matrix3x3.h"
#include "Math/Vector2.h"
#include <cmath>

namespace KashipanEngine {
namespace MathUtils {

Matrix3x3 Matrix3x3Identity() noexcept {
    return Matrix3x3(
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}

Matrix3x3 Matrix3x3Transpose(const Matrix3x3 &matrix) noexcept {
    return Matrix3x3(
        matrix.m[0][0], matrix.m[1][0], matrix.m[2][0],
        matrix.m[0][1], matrix.m[1][1], matrix.m[2][1],
        matrix.m[0][2], matrix.m[1][2], matrix.m[2][2]
    );
}

float Matrix3x3Determinant(const Matrix3x3 &matrix) noexcept {
    Matrix3x3::Matrix2x2 c00(matrix.m[1][1], matrix.m[1][2], matrix.m[2][1], matrix.m[2][2]);
    Matrix3x3::Matrix2x2 c01(matrix.m[1][0], matrix.m[1][2], matrix.m[2][0], matrix.m[2][2]);
    Matrix3x3::Matrix2x2 c02(matrix.m[1][0], matrix.m[1][1], matrix.m[2][0], matrix.m[2][1]);
    return matrix.m[0][0] * c00.Determinant() - matrix.m[0][1] * c01.Determinant() + matrix.m[0][2] * c02.Determinant();
}

Matrix3x3 Matrix3x3Inverse(const Matrix3x3 &matrix) {
    using M2 = Matrix3x3::Matrix2x2;

    float c00 = M2(matrix.m[1][1], matrix.m[1][2], matrix.m[2][1], matrix.m[2][2]).Determinant();
    float c01 = -M2(matrix.m[1][0], matrix.m[1][2], matrix.m[2][0], matrix.m[2][2]).Determinant();
    float c02 = M2(matrix.m[1][0], matrix.m[1][1], matrix.m[2][0], matrix.m[2][1]).Determinant();

    float c10 = -M2(matrix.m[0][1], matrix.m[0][2], matrix.m[2][1], matrix.m[2][2]).Determinant();
    float c11 = M2(matrix.m[0][0], matrix.m[0][2], matrix.m[2][0], matrix.m[2][2]).Determinant();
    float c12 = -M2(matrix.m[0][0], matrix.m[0][1], matrix.m[2][0], matrix.m[2][1]).Determinant();

    float c20 = M2(matrix.m[0][1], matrix.m[0][2], matrix.m[1][1], matrix.m[1][2]).Determinant();
    float c21 = -M2(matrix.m[0][0], matrix.m[0][2], matrix.m[1][0], matrix.m[1][2]).Determinant();
    float c22 = M2(matrix.m[0][0], matrix.m[0][1], matrix.m[1][0], matrix.m[1][1]).Determinant();

    float det = matrix.m[0][0] * c00 + matrix.m[0][1] * c01 + matrix.m[0][2] * c02;
    if (det == 0.0f) {
        // 非正則の場合は単位行列を返す
        return Matrix3x3Identity();
    }
    float invDet = 1.0f / det;

    return Matrix3x3(
        c00 * invDet, c10 * invDet, c20 * invDet,
        c01 * invDet, c11 * invDet, c21 * invDet,
        c02 * invDet, c12 * invDet, c22 * invDet
    );
}

Matrix3x3 Matrix3x3MakeTranslate(const Vector2 &translate) noexcept {
    return Matrix3x3(
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        translate.x, translate.y, 1.0f
    );
}

Matrix3x3 Matrix3x3MakeScale(const Vector2 &scale) noexcept {
    return Matrix3x3(
        scale.x, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}

Matrix3x3 Matrix3x3MakeRotate(float radian) noexcept {
    float c = std::cos(radian);
    float s = std::sin(radian);

    return Matrix3x3(
        c, s, 0.0f,
        -s, c, 0.0f,
        0.0f, 0.0f, 1.0f
    );
}

Matrix3x3 Matrix3x3MakeAffine(const Vector2 &scale, float radian, const Vector2 &translate) noexcept {
    Matrix3x3 s = Matrix3x3MakeScale(scale);
    Matrix3x3 r = Matrix3x3MakeRotate(radian);
    Matrix3x3 t = Matrix3x3MakeTranslate(translate);
    return s * r * t;
}

} // namespace MathUtils
} // namespace KashipanEngine