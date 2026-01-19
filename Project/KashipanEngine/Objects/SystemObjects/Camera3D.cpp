#include "Objects/SystemObjects/Camera3D.h"
#include <cstring>
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

Camera3D::Camera3D()
    : Object3DBase("Camera3D") {
    SetRenderType(RenderType::Standard);
    // GPU constant buffer is now owned by Renderer

    // 既存シェーダ側のデフォルトに寄せた初期値
    fovY_ = 0.45f;
    aspectRatio_ = 16.0f / 9.0f;
    nearClip_ = 0.1f;
    farClip_ = 2048.0f;

    // Orthographic defaults (world space)
    orthoLeft_ = -10.0f;
    orthoTop_ = 10.0f;
    orthoRight_ = 10.0f;
    orthoBottom_ = -10.0f;

    viewportLeft_ = 0.0f;
    viewportTop_ = 0.0f;
    viewportWidth_ = 1280.0f;
    viewportHeight_ = 720.0f;
    viewportMinDepth_ = 0.0f;
    viewportMaxDepth_ = 1.0f;

    std::vector<RenderPass::ConstantBufferRequirement> reqs;
    for (const auto &key : constantBufferRequirementKeys_) {
        reqs.push_back({ key, sizeof(CameraBuffer) });
    }
    SetConstantBufferRequirements(reqs);
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            UpdateCameraBufferCPU();
            auto **maps = static_cast<void **>(constantBufferMaps);
            std::memcpy(maps[0], &cameraBufferCPU_, sizeof(CameraBuffer));
            return true;
        });
}

void Camera3D::SetFovY(float fovY) {
    fovY_ = fovY;

    if (cameraType_ == CameraType::Perspective) {
        isProjectionMatrixCalculated_ = false;
        isViewProjectionMatrixCalculated_ = false;
    }
}

void Camera3D::SetAspectRatio(float aspectRatio) {
    aspectRatio_ = aspectRatio;

    if (cameraType_ == CameraType::Perspective) {
        isProjectionMatrixCalculated_ = false;
        isViewProjectionMatrixCalculated_ = false;
    }
}

void Camera3D::SetNearClip(float nearClip) {
    nearClip_ = nearClip;

    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
}

void Camera3D::SetFarClip(float farClip) {
    farClip_ = farClip;

    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
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

    if (cameraType_ == CameraType::Perspective) {
        isProjectionMatrixCalculated_ = false;
        isViewProjectionMatrixCalculated_ = false;
    }
}

void Camera3D::SetOrthographicParams(float left, float top, float right, float bottom, float nearClip, float farClip) {
    orthoLeft_ = left;
    orthoTop_ = top;
    orthoRight_ = right;
    orthoBottom_ = bottom;
    nearClip_ = nearClip;
    farClip_ = farClip;

    if (cameraType_ == CameraType::Orthographic) {
        isProjectionMatrixCalculated_ = false;
        isViewProjectionMatrixCalculated_ = false;
    }
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

const Matrix4x4 &Camera3D::GetViewMatrix() const {
    if (auto *t = GetComponent3D<Transform3D>()) {
        // Transformの変更検知は「worldMatrixがdirtyか」よりも、値のスナップショットで行う。
        // GetWorldMatrix() を呼ぶと dirty フラグが解消されてしまうため、
        // 呼び出し順序によっては View の更新が抑止され得る。
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

const Matrix4x4 &Camera3D::GetProjectionMatrix() const {
    if (!isProjectionMatrixCalculated_) {
        if (cameraType_ == CameraType::Perspective) {
            projectionMatrix_.MakePerspectiveFovMatrix(
                fovY_, aspectRatio_, nearClip_, farClip_);
        } else if (cameraType_ == CameraType::Orthographic) {
            projectionMatrix_.MakeOrthographicMatrix(
                orthoLeft_, orthoTop_, orthoRight_, orthoBottom_,
                nearClip_, farClip_);
        } else {
            projectionMatrix_ = Matrix4x4::Identity();
        }
        isProjectionMatrixCalculated_ = true;
    }
    return projectionMatrix_;
}

const Matrix4x4 &Camera3D::GetViewProjectionMatrix() const {
    if (!isViewProjectionMatrixCalculated_) {
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

void Camera3D::SetCameraType(CameraType type) {
    if (cameraType_ == type) return;
    cameraType_ = type;
    isProjectionMatrixCalculated_ = false;
    isViewProjectionMatrixCalculated_ = false;
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

void Camera3D::SetConstantBufferRequirementKeys(const std::vector<std::string> &keys) {
    constantBufferRequirementKeys_ = keys;
    std::vector<RenderPass::ConstantBufferRequirement> reqs;
    for (const auto &key : constantBufferRequirementKeys_) {
        reqs.push_back({ key, sizeof(CameraBuffer) });
    }
    SetConstantBufferRequirements(reqs);
}

bool Camera3D::Render(ShaderVariableBinder &shaderBinder) {
    (void)shaderBinder;
    UpdateCameraBufferCPU();
    return true;
}

} // namespace KashipanEngine
