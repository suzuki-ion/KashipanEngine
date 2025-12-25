#pragma once
#include <memory>
#include "Objects/Object3DBase.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class Camera3D final : public Object3DBase {
public:

    Camera3D();
    ~Camera3D() override = default;

    void SetView(const Matrix4x4 &view);
    void SetProjection(const Matrix4x4 &projection);
    void SetViewProjection(const Matrix4x4 &viewProjection);
    void SetFov(float fov);

    const Matrix4x4 &GetPerspectiveMatrix() const;
    const Matrix4x4 &GetViewMatrix() const;
    const Matrix4x4 &GetProjectionMatrix() const;
    const Matrix4x4 &GetViewProjectionMatrix() const;
    const Matrix4x4 &GetViewportMatrix() const;

    void SetFovY(float fovY);
    void SetAspectRatio(float aspectRatio);
    void SetNearClip(float nearClip);
    void SetFarClip(float farClip);
    float GetFovY() const { return fovY_; }
    float GetAspectRatio() const { return aspectRatio_; }
    float GetNearClip() const { return nearClip_; }
    float GetFarClip() const { return farClip_; }

    void SetViewportLeft(float left);
    void SetViewportTop(float top);
    void SetViewportWidth(float width);
    void SetViewportHeight(float height);
    void SetViewportMinDepth(float minDepth);
    void SetViewportMaxDepth(float maxDepth);
    float GetViewportLeft() const { return viewportLeft_; }
    float GetViewportTop() const { return viewportTop_; }
    float GetViewportWidth() const { return viewportWidth_; }
    float GetViewportHeight() const { return viewportHeight_; }
    float GetViewportMinDepth() const { return viewportMinDepth_; }
    float GetViewportMaxDepth() const { return viewportMaxDepth_; }

    void SetPerspectiveParams(float fovY, float aspectRatio, float nearClip, float farClip);
    void SetViewportParams(float left, float top, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);

protected:
    bool Render(ShaderVariableBinder &shaderBinder) override;

private:
    struct CameraBuffer {
        Matrix4x4 view{};
        Matrix4x4 projection{};
        Matrix4x4 viewProjection{};
        Vector4 eyePosition{};
        float fov = 0.0f;
    };

    void Upload() const;
    void UpdateCameraBufferCPU() const;

    // Perspective
    float fovY_ = 0.0f;
    float aspectRatio_ = 1.0f;
    float nearClip_ = 0.1f;
    float farClip_ = 2048.0f;

    // Viewport
    float viewportLeft_ = 0.0f;
    float viewportTop_ = 0.0f;
    float viewportWidth_ = 1.0f;
    float viewportHeight_ = 1.0f;
    float viewportMinDepth_ = 0.0f;
    float viewportMaxDepth_ = 1.0f;

    // Matrices (lazy)
    mutable Matrix4x4 perspectiveMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 viewMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 projectionMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 viewProjectionMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 viewportMatrix_ = Matrix4x4::Identity();

    mutable bool isPerspectiveMatrixCalculated_ = false;
    mutable bool isViewMatrixCalculated_ = false;
    mutable bool isProjectionMatrixCalculated_ = false;
    mutable bool isViewProjectionMatrixCalculated_ = false;
    mutable bool isViewportMatrixCalculated_ = false;

    // 互換APIから注入された場合の優先フラグ
    mutable bool isOverrideView_ = false;
    mutable bool isOverrideProjection_ = false;
    mutable bool isOverrideViewProjection_ = false;

    mutable CameraBuffer cameraBufferCPU_{};
    std::unique_ptr<ConstantBufferResource> cameraBufferGPU_;

    // Transformキャッシュ（カメラの view 再計算のため）
    mutable Vector3 lastTransformTranslate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformRotate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformScale_{1.0f, 1.0f, 1.0f};
};

} // namespace KashipanEngine
