#pragma once
#include <memory>
#include "Objects/Object2DBase.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class Camera2D final : public Object2DBase {
public:
    Camera2D();
    ~Camera2D() override = default;

    const Matrix4x4 &GetViewMatrix() const;
    const Matrix4x4 &GetProjectionMatrix() const;
    const Matrix4x4 &GetViewProjectionMatrix() const;
    const Matrix4x4 &GetViewportMatrix() const;

    void SetLeft(float left);
    void SetTop(float top);
    void SetRight(float right);
    void SetBottom(float bottom);
    void SetNearClip(float nearClip);
    void SetFarClip(float farClip);

    float GetLeft() const { return left_; }
    float GetTop() const { return top_; }
    float GetRight() const { return right_; }
    float GetBottom() const { return bottom_; }
    float GetNearClip() const { return nearClip_; }
    float GetFarClip() const { return farClip_; }

    void SetOrthographicParams(float left, float top, float right, float bottom, float nearClip, float farClip);

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

    void SetViewportParams(float left, float top, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);

protected:
    bool Render(ShaderVariableBinder &shaderBinder) override;

private:
    struct CameraBuffer {
        Matrix4x4 view{};
        Matrix4x4 projection{};
        Matrix4x4 viewProjection{};
    };

    void Upload() const;
    void UpdateCameraBufferCPU() const;

    // Ortho
    float left_ = 0.0f;
    float top_ = 0.0f;
    float right_ = 1280.0f;
    float bottom_ = 720.0f;
    float nearClip_ = 0.0f;
    float farClip_ = 1.0f;

    // Viewport
    float viewportLeft_ = 0.0f;
    float viewportTop_ = 0.0f;
    float viewportWidth_ = 1280.0f;
    float viewportHeight_ = 720.0f;
    float viewportMinDepth_ = 0.0f;
    float viewportMaxDepth_ = 1.0f;

    // Matrices
    mutable Matrix4x4 viewMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 projectionMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 viewProjectionMatrix_ = Matrix4x4::Identity();
    mutable Matrix4x4 viewportMatrix_ = Matrix4x4::Identity();

    mutable bool isViewMatrixCalculated_ = false;
    mutable bool isProjectionMatrixCalculated_ = false;
    mutable bool isViewProjectionMatrixCalculated_ = false;
    mutable bool isViewportMatrixCalculated_ = false;

    mutable CameraBuffer cameraBufferCPU_{};
    std::unique_ptr<ConstantBufferResource> cameraBufferGPU_;

    // Transformキャッシュ（カメラの view 再計算のため）
    mutable Vector3 lastTransformTranslate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformRotate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformScale_{1.0f, 1.0f, 1.0f};
};

} // namespace KashipanEngine
