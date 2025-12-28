#include "Objects/SystemObjects/Camera2D.h"
#include <cstring>
#include "Objects/Components/2D/Transform2D.h"

namespace KashipanEngine {

Camera2D::Camera2D()
    : Object2DBase("Camera2D") {
    SetRenderType(RenderType::Standard);

    // 既存の 2D 系初期値に寄せる（ピクセル座標ベース）
    left_ = 0.0f;
    top_ = 0.0f;
    right_ = 1280.0f;
    bottom_ = 720.0f;
    nearClip_ = 0.0f;
    farClip_ = 1.0f;

    viewportLeft_ = 0.0f;
    viewportTop_ = 0.0f;
    viewportWidth_ = 1280.0f;
    viewportHeight_ = 720.0f;
    viewportMinDepth_ = 0.0f;
    viewportMaxDepth_ = 1.0f;
}

void Camera2D::SetLeft(float left) {
    left_ = left;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetTop(float top) {
    top_ = top;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetRight(float right) {
    right_ = right;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetBottom(float bottom) {
    bottom_ = bottom;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetNearClip(float nearClip) {
    nearClip_ = nearClip;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetFarClip(float farClip) {
    farClip_ = farClip;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetOrthographicParams(float left, float top, float right, float bottom, float nearClip, float farClip) {
    left_ = left;
    top_ = top;
    right_ = right;
    bottom_ = bottom;
    nearClip_ = nearClip;
    farClip_ = farClip;

    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera2D::SetViewportLeft(float left) {
    viewportLeft_ = left;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportTop(float top) {
    viewportTop_ = top;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportWidth(float width) {
    viewportWidth_ = width;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportHeight(float height) {
    viewportHeight_ = height;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportMinDepth(float minDepth) {
    viewportMinDepth_ = minDepth;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportMaxDepth(float maxDepth) {
    viewportMaxDepth_ = maxDepth;
    isViewportMatrixCalculated_ = false;
}

void Camera2D::SetViewportParams(float left, float top, float width, float height, float minDepth, float maxDepth) {
    viewportLeft_ = left;
    viewportTop_ = top;
    viewportWidth_ = width;
    viewportHeight_ = height;
    viewportMinDepth_ = minDepth;
    viewportMaxDepth_ = maxDepth;

    isViewportMatrixCalculated_ = false;
}

const Matrix4x4 &Camera2D::GetViewMatrix() const {
    if (auto *t = GetComponent2D<Transform2D>()) {
        const Vector3 curTranslate = t->GetTranslate();
        const Vector3 curRotate = t->GetRotate();
        const Vector3 curScale = t->GetScale();

        const bool transformChanged =
            (curTranslate != lastTransformTranslate_) ||
            (curRotate != lastTransformRotate_) ||
            (curScale != lastTransformScale_);

        if (transformChanged) {
            isViewMatrixCalculated_ = false;
        }

        if (!isViewMatrixCalculated_) {
            viewMatrix_ = t->GetWorldMatrix().Inverse();
            isViewMatrixCalculated_ = true;
            isViewProjectionMatrixCalculated_ = false;

            lastTransformTranslate_ = curTranslate;
            lastTransformRotate_ = curRotate;
            lastTransformScale_ = curScale;
        }
    } else {
        if (!isViewMatrixCalculated_) {
            viewMatrix_ = Matrix4x4::Identity();
            isViewMatrixCalculated_ = true;
        }
    }

    return viewMatrix_;
}

const Matrix4x4 &Camera2D::GetProjectionMatrix() const {
    if (!isProjectionMatrixCalculated_) {
        projectionMatrix_ = Matrix4x4::Identity();
        projectionMatrix_.MakeOrthographicMatrix(left_, top_, right_, bottom_, nearClip_, farClip_);
        isProjectionMatrixCalculated_ = true;
    }
    return projectionMatrix_;
}

const Matrix4x4 &Camera2D::GetViewProjectionMatrix() const {
    if (!isViewProjectionMatrixCalculated_) {
        viewProjectionMatrix_ = GetViewMatrix() * GetProjectionMatrix();
        isViewProjectionMatrixCalculated_ = true;
    }
    return viewProjectionMatrix_;
}

const Matrix4x4 &Camera2D::GetViewportMatrix() const {
    if (!isViewportMatrixCalculated_) {
        viewportMatrix_ = Matrix4x4::Identity();
        viewportMatrix_.MakeViewportMatrix(viewportLeft_, viewportTop_, viewportWidth_, viewportHeight_, viewportMinDepth_, viewportMaxDepth_);
        isViewportMatrixCalculated_ = true;
    }
    return viewportMatrix_;
}

void Camera2D::UpdateCameraBufferCPU() const {
    cameraBufferCPU_.view = GetViewMatrix();
    cameraBufferCPU_.projection = GetProjectionMatrix();
    cameraBufferCPU_.viewProjection = GetViewProjectionMatrix();
}

bool Camera2D::Render(ShaderVariableBinder &shaderBinder) {
    (void)shaderBinder;
    UpdateCameraBufferCPU();
    return true;
}

} // namespace KashipanEngine
