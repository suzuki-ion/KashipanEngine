#include "Objects/SystemObjects/Camera3D.h"
#include <cstring>
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

Camera3D::Camera3D()
    : Object3DBase("Camera3D") {
    cameraBufferGPU_ = std::make_unique<ConstantBufferResource>(sizeof(CameraBuffer));

    // 既存シェーダ側のデフォルトに寄せた初期値
    fovY_ = 0.45f;
    aspectRatio_ = 16.0f / 9.0f;
    nearClip_ = 0.1f;
    farClip_ = 2048.0f;

    viewportLeft_ = 0.0f;
    viewportTop_ = 0.0f;
    viewportWidth_ = 1280.0f;
    viewportHeight_ = 720.0f;
    viewportMinDepth_ = 0.0f;
    viewportMaxDepth_ = 1.0f;
}

void Camera3D::SetFovY(float fovY) {
    fovY_ = fovY;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
    
    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;

    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetNearClip(float nearClip) {
    nearClip_ = nearClip;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;

    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetFarClip(float farClip) {
    farClip_ = farClip;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;

    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetViewportLeft(float left) {
    viewportLeft_ = left;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetViewportTop(float top) {
    viewportTop_ = top;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetViewportWidth(float width) {
    viewportWidth_ = width;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetViewportHeight(float height) {
    viewportHeight_ = height;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetViewportMinDepth(float minDepth) {
    viewportMinDepth_ = minDepth;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetViewportMaxDepth(float maxDepth) {
    viewportMaxDepth_ = maxDepth;
    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetPerspectiveParams(float fovY, float aspectRatio, float nearClip, float farClip) {
    fovY_ = fovY;
    aspectRatio_ = aspectRatio;
    nearClip_ = nearClip;
    farClip_ = farClip;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;

    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetViewportParams(float left, float top, float width, float height, float minDepth, float maxDepth) {
    viewportLeft_ = left;
    viewportTop_ = top;
    viewportWidth_ = width;
    viewportHeight_ = height;
    viewportMinDepth_ = minDepth;
    viewportMaxDepth_ = maxDepth;

    isViewportMatrixCalculated_ = false;
}

void Camera3D::SetView(const Matrix4x4 &view) {
    viewMatrix_ = view;
    isViewMatrixCalculated_ = true;
    isOverrideView_ = true;

    isViewProjectionMatrixCalculated_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetProjection(const Matrix4x4 &projection) {
    projectionMatrix_ = projection;
    isProjectionMatrixCalculated_ = true;
    isOverrideProjection_ = true;

    isViewProjectionMatrixCalculated_ = false;
    isOverrideViewProjection_ = false;
}

void Camera3D::SetViewProjection(const Matrix4x4 &viewProjection) {
    viewProjectionMatrix_ = viewProjection;
    isViewProjectionMatrixCalculated_ = true;
    isOverrideViewProjection_ = true;

    // viewProjectionを直接注入する場合は、個別のview/projection上書きは無効化して整合性を取る
    isOverrideView_ = false;
    isOverrideProjection_ = false;
    isViewMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
}

void Camera3D::SetFov(float fov) {
    fovY_ = fov;

    isPerspectiveMatrixCalculated_ = false;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;

    isOverrideProjection_ = false;
    isOverrideViewProjection_ = false;
}

const Matrix4x4 &Camera3D::GetPerspectiveMatrix() const {
    if (!isPerspectiveMatrixCalculated_) {
        perspectiveMatrix_ = Matrix4x4::Identity();
        perspectiveMatrix_.MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
        isPerspectiveMatrixCalculated_ = true;
    }
    return perspectiveMatrix_;
}

const Matrix4x4 &Camera3D::GetViewMatrix() const {
    if (isOverrideView_) {
        isViewMatrixCalculated_ = true;
        return viewMatrix_;
    }

    if (auto *t = GetComponent3D<Transform3D>()) {
        // Transformが更新された場合、viewのキャッシュを無効化する
        if (t->IsWorldMatrixDirty()) {
            isViewMatrixCalculated_ = false;
        }

        if (!isViewMatrixCalculated_) {
            viewMatrix_ = t->GetWorldMatrix().Inverse();
            isViewMatrixCalculated_ = true;
        }
    } else {
        if (!isViewMatrixCalculated_) {
            viewMatrix_ = Matrix4x4::Identity();
            isViewMatrixCalculated_ = true;
        }
    }

    return viewMatrix_;
}

const Matrix4x4 &Camera3D::GetProjectionMatrix() const {
    if (!isProjectionMatrixCalculated_) {
        if (isOverrideProjection_) {
            isProjectionMatrixCalculated_ = true;
            return projectionMatrix_;
        }
        projectionMatrix_ = GetPerspectiveMatrix();
        isProjectionMatrixCalculated_ = true;
    }
    return projectionMatrix_;
}

const Matrix4x4 &Camera3D::GetViewProjectionMatrix() const {
    if (!isViewProjectionMatrixCalculated_) {
        if (isOverrideViewProjection_) {
            isViewProjectionMatrixCalculated_ = true;
            return viewProjectionMatrix_;
        }
        viewProjectionMatrix_ = GetViewMatrix() * GetProjectionMatrix();
        isViewProjectionMatrixCalculated_ = true;
    }
    return viewProjectionMatrix_;
}

const Matrix4x4 &Camera3D::GetViewportMatrix() const {
    if (!isViewportMatrixCalculated_) {
        viewportMatrix_ = Matrix4x4::Identity();
        viewportMatrix_.MakeViewportMatrix(viewportLeft_, viewportTop_, viewportWidth_, viewportHeight_, viewportMinDepth_, viewportMaxDepth_);
        isViewportMatrixCalculated_ = true;
    }
    return viewportMatrix_;
}

void Camera3D::UpdateCameraBufferCPU() const {
    cameraBufferCPU_.view = GetViewMatrix();
    cameraBufferCPU_.projection = GetProjectionMatrix();
    cameraBufferCPU_.viewProjection = GetViewProjectionMatrix();
    if (auto *t = GetComponent3D<Transform3D>()) {
        const Matrix4x4 &worldMat = t->GetWorldMatrix();
        cameraBufferCPU_.eyePosition.x = worldMat.m[3][0];
        cameraBufferCPU_.eyePosition.y = worldMat.m[3][1];
        cameraBufferCPU_.eyePosition.z = worldMat.m[3][2];
        cameraBufferCPU_.eyePosition.w = 1.0f;
    } else {
        cameraBufferCPU_.eyePosition.x = 0.0f;
        cameraBufferCPU_.eyePosition.y = 0.0f;
        cameraBufferCPU_.eyePosition.z = 0.0f;
        cameraBufferCPU_.eyePosition.w = 1.0f;
    }
    cameraBufferCPU_.fov = fovY_;
}

void Camera3D::Upload() const {
    if (!cameraBufferGPU_) return;
    void *mapped = cameraBufferGPU_->Map();
    if (!mapped) return;
    std::memcpy(mapped, &cameraBufferCPU_, sizeof(CameraBuffer));
    cameraBufferGPU_->Unmap();
}

bool Camera3D::Render(ShaderVariableBinder &shaderBinder) {
    if (!cameraBufferGPU_) return false;

    // Render時点で最新状態をgCameraへ反映
    UpdateCameraBufferCPU();
    Upload();

    return shaderBinder.Bind("Vertex:gCamera", cameraBufferGPU_.get());
}

} // namespace KashipanEngine
