#include "SRTMatrix4x4.h"

namespace KashipanEngine {

SRTMatrix4x4::SRTMatrix4x4(const Matrix4x4 &scale, const Matrix4x4 &rotate, const Matrix4x4 &translate) noexcept {
    scaleMatrix = scale;
    rotateMatrix = rotate;
    translateMatrix = translate;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

SRTMatrix4x4::SRTMatrix4x4(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) noexcept {
    scaleMatrix.MakeScale(scale);
    rotateMatrix.MakeRotate(rotate);
    translateMatrix.MakeTranslate(translate);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

SRTMatrix4x4::SRTMatrix4x4(const SRTMatrix4x4 &affine) noexcept {
    scaleMatrix = affine.GetScaleMatrix();
    rotateMatrix = affine.rotateMatrix;
    translateMatrix = affine.translateMatrix;
    worldMatrix = affine.worldMatrix;
}

SRTMatrix4x4 &SRTMatrix4x4::operator=(const SRTMatrix4x4 &affine) noexcept {
    scaleMatrix = affine.scaleMatrix;
    rotateMatrix = affine.rotateMatrix;
    translateMatrix = affine.translateMatrix;
    worldMatrix = affine.worldMatrix;
    return *this;
}

SRTMatrix4x4 &SRTMatrix4x4::operator*=(const SRTMatrix4x4 &affine) noexcept {
    scaleMatrix *= affine.scaleMatrix;
    rotateMatrix *= affine.rotateMatrix;
    translateMatrix *= affine.translateMatrix;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
    return *this;
}

SRTMatrix4x4 &SRTMatrix4x4::operator*=(const Matrix4x4 &mat) noexcept {
    scaleMatrix *= mat;
    rotateMatrix *= mat;
    translateMatrix *= mat;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
    return *this;
}

void SRTMatrix4x4::SetSRT(const Vector3 &scale, const Vector3 &rotate, const Vector3 &translate) {
    scaleMatrix.MakeScale(scale);
    rotateMatrix.MakeRotate(rotate);
    translateMatrix.MakeTranslate(translate);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetScale(const Vector3 &scale) noexcept {
    scaleMatrix.MakeScale(scale);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetScale(const Matrix4x4 &scale) noexcept {
    scaleMatrix = scale;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetScale(float scaleX, float scaleY, float scaleZ) noexcept {
    scaleMatrix.MakeScale({ scaleX, scaleY, scaleZ });
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetScale(float scale) noexcept {
    scaleMatrix.MakeScale({ scale, scale, scale });
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetRotate(const Vector3 &rotate) noexcept {
    rotateMatrix.MakeRotate(rotate);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetRotate(const Matrix4x4 &rotate) noexcept {
    rotateMatrix = rotate;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetRotate(float radianX, float radianY, float radianZ) noexcept {
    rotateMatrix.MakeRotate(radianX, radianY, radianZ);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetTranslate(const Vector3 &translate) noexcept {
    translateMatrix.MakeTranslate(translate);
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetTranslate(const Matrix4x4 &translate) noexcept {
    translateMatrix = translate;
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

void SRTMatrix4x4::SetTranslate(float translateX, float translateY, float translateZ) noexcept {
    translateMatrix.MakeTranslate({ translateX, translateY, translateZ });
    worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
}

Matrix4x4 SRTMatrix4x4::InverseScale() const noexcept {
    return Matrix4x4(
        1.0f / scaleMatrix.m[0][0], 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f / scaleMatrix.m[1][1], 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f / scaleMatrix.m[2][2], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 SRTMatrix4x4::InverseRotate() const noexcept {
    return Matrix4x4(
        rotateMatrix.m[0][0], rotateMatrix.m[1][0], rotateMatrix.m[2][0], 0.0f,
        rotateMatrix.m[0][1], rotateMatrix.m[1][1], rotateMatrix.m[2][1], 0.0f,
        rotateMatrix.m[0][2], rotateMatrix.m[1][2], rotateMatrix.m[2][2], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

Matrix4x4 SRTMatrix4x4::InverseTranslate() const noexcept {
    return Matrix4x4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -translateMatrix.m[3][0], -translateMatrix.m[3][1], -translateMatrix.m[3][2], 1.0f
    );
}

} // namespace KashipanEngine