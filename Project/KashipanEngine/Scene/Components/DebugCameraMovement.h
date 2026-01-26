#pragma once
#include "Scene/Components/ISceneComponent.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Input/Input.h"
#include "Utilities/MathUtils.h"
#ifdef USE_IMGUI
#include <imgui.h>
#include <numbers>
#endif

namespace KashipanEngine {

class SceneContext;

class DebugCameraMovement : public ISceneComponent {
public:
    DebugCameraMovement(Camera3D *camera, Input *input);
    void Update() override;
    void SetOwnerContext(SceneContext *context) { ownerContext_ = context; }

    void SetCenter(const Vector3 &center);
    void SetDistance(float distance);
    void SetAngles(float theta, float phi);
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }
    void SetRotateSpeed(float speed) { rotateSpeed_ = speed; }
    void SetZoomSpeed(float speed) { zoomSpeed_ = speed; }
    float GetMoveSpeed() const { return moveSpeed_; }
    float GetRotateSpeed() const { return rotateSpeed_; }
    float GetZoomSpeed() const { return zoomSpeed_; }

    void SetEnable(bool enable) { isEnable_ = enable; }
    bool IsEnable() const { return isEnable_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Text("DebugCameraMovement");
        ImGui::DragFloat3("Center", &spherical_.origin.x, 0.1f);
        ImGui::DragFloat("Distance", &spherical_.radius, 0.1f, 0.1f, 1000.0f);
        ImGui::DragFloat("Theta", &spherical_.theta, 0.01f, M_PI * 2.0f, M_PI * 2.0f);
        ImGui::DragFloat("Phi", &spherical_.phi, 0.01f, 0.01f, M_PI - 0.01f);
        ImGui::DragFloat("Move Speed", &moveSpeed_, 0.001f, 0.001f, 10.0f, "%.3f");
        ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.001f, 0.001f, 1.0f, "%.3f");
        ImGui::DragFloat("Zoom Speed", &zoomSpeed_, 0.001f, 0.001f, 2.0f, "%.3f");
    }
#endif

private:
    Camera3D *camera_;
    Input *input_;
    SceneContext *ownerContext_ = nullptr;
    SphericalCoordinates spherical_;
    POINT lastMousePos_ = { 0, 0 };

    float moveSpeed_ = 0.001f;
    float rotateSpeed_ = 0.01f;
    float zoomSpeed_ = 0.001f;

    bool isEnable_ = false;
    
    bool isIgnoreWheelOnFocus_ = false;
    bool isLastWindowFocused_ = false;

    void HandleInput();
    void UpdateCamera();
};

} // namespace KashipanEngine
