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

#if defined(USE_IMGUI)
    void ShowImGuiDerived() override {
        ImGui::TextUnformatted(Translation("engine.imgui.camera3d.params").c_str());

        float fov = fovY_;
        float aspect = aspectRatio_;
        float nearC = nearClip_;
        float farC = farClip_;

        ImGui::DragFloat(Translation("engine.imgui.camera3d.fovY").c_str(), &fov, 0.01f, 0.01f, 6.28f);
        ImGui::DragFloat(Translation("engine.imgui.camera3d.aspect").c_str(), &aspect, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera3d.near").c_str(), &nearC, 0.01f, 0.001f, 1000.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera3d.far").c_str(), &farC, 1.0f, 0.01f, 100000.0f);
        SetPerspectiveParams(fov, aspect, nearC, farC);

        ImGui::Separator();
        ImGui::TextUnformatted(Translation("engine.imgui.camera3d.viewport").c_str());

        float l = viewportLeft_;
        float t = viewportTop_;
        float w = viewportWidth_;
        float h = viewportHeight_;
        float minD = viewportMinDepth_;
        float maxD = viewportMaxDepth_;

        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.left").c_str(), &l, 1.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.top").c_str(), &t, 1.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.width").c_str(), &w, 1.0f, 1.0f, 100000.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.height").c_str(), &h, 1.0f, 1.0f, 100000.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.minDepth").c_str(), &minD, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat(Translation("engine.imgui.camera.viewport.maxDepth").c_str(), &maxD, 0.01f, 0.0f, 1.0f);
        SetViewportParams(l, t, w, h, minD, maxD);
    }
#endif

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

    // Matrices
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

    mutable CameraBuffer cameraBufferCPU_{};
    std::unique_ptr<ConstantBufferResource> cameraBufferGPU_;

    // Transformキャッシュ（カメラの view 再計算のため）
    mutable Vector3 lastTransformTranslate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformRotate_{0.0f, 0.0f, 0.0f};
    mutable Vector3 lastTransformScale_{1.0f, 1.0f, 1.0f};
};

} // namespace KashipanEngine
