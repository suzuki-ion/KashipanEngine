#include "Scene/Components/DebugCameraMovement.h"
#include "Scene/SceneContext.h"
#include "Scene/Components/SceneDefaultVariables.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "InputHeaders.h"
#include "Core/Window.h"
#include "Math/Vector3.h"
#include "Math/Matrix4x4.h"
#include "Utilities/MathUtils.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace KashipanEngine {

DebugCameraMovement::DebugCameraMovement(Camera3D* camera, Input* input)
    : ISceneComponent("DebugCameraMovement", 1), camera_(camera), input_(input) {
    spherical_.radius = 10.0f;
    spherical_.theta = 0.0f;
    spherical_.phi = M_PI / 2.0f;
    spherical_.origin = {0, 0, 0};
}

void DebugCameraMovement::SetCenter(const Vector3& center) {
    spherical_.origin = center;
}
void DebugCameraMovement::SetDistance(float distance) {
    spherical_.radius = std::max(0.1f, distance);
}
void DebugCameraMovement::SetAngles(float theta, float phi) {
    spherical_.theta = theta;
    spherical_.phi = std::clamp(phi, 0.01f, M_PI - 0.01f);
}

void DebugCameraMovement::Update() {
    if (!isEnable_) return;
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
    HandleInput();
    UpdateCamera();
#endif
}

void DebugCameraMovement::HandleInput() {
#ifdef USE_IMGUI
    if (ImGui::IsAnyItemActive() || ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || ImGui::IsAnyItemHovered()) return;
#endif
    auto *ownerContext = GetOwnerContext();
    if (!input_ || !ownerContext) return;
    
    auto* sceneVars = ownerContext->GetComponent<SceneDefaultVariables>("SceneDefaultVariables");
    if (!sceneVars) return;
    
    auto *window = sceneVars->GetMainWindow();
    if (!window) return;

    // フォーカス状態の変化を検出
    bool nowFocused = window->IsFocused();
    if (nowFocused && !isLastWindowFocused_) {
        isIgnoreWheelOnFocus_ = true; // 復帰直後はホイール値を無視
    }
    isLastWindowFocused_ = nowFocused;

    if (!nowFocused) return;

    auto &mouse = input_->GetMouse();
    int wheel = mouse.GetWheel();

    if (isIgnoreWheelOnFocus_) {
        wheel = 0;
        isIgnoreWheelOnFocus_ = false;
    }

    auto& keyboard = input_->GetKeyboard();
    bool dragging = mouse.IsButtonDown(2);
    bool shift = keyboard.IsDown(Key::Shift);
    POINT mousePos = mouse.GetPos(window->GetWindowHandle());
    int dx = mousePos.x - lastMousePos_.x;
    int dy = mousePos.y - lastMousePos_.y;
    lastMousePos_ = mousePos;

    if (wheel != 0 && !dragging) {
        float adjustSpeed = zoomSpeed_ * spherical_.radius;
        spherical_.radius += -wheel * adjustSpeed;
        spherical_.radius = std::max(0.1f, spherical_.radius);
    }
    if (dragging && !shift) {
        spherical_.theta -= dx * rotateSpeed_;
        spherical_.phi   -= dy * rotateSpeed_;
        spherical_.phi = std::clamp(spherical_.phi, 0.01f, M_PI - 0.01f);
    }
    if (dragging && shift) {
        float move = moveSpeed_ * spherical_.radius;
        float up = dy * move;
        float right = -dx * move;
        Vector3 eye = spherical_.ToVector3();
        Vector3 at = spherical_.origin;
        Vector3 forward = (at - eye).Normalize();
        Vector3 worldUp(0, 1, 0);
        Vector3 rightv = worldUp.Cross(forward).Normalize();
        Vector3 upv = forward.Cross(rightv).Normalize();
        spherical_.origin = spherical_.origin + rightv * right + upv * up;
    }
}

void DebugCameraMovement::UpdateCamera() {
    if (!camera_) return;
    Vector3 pos = spherical_.ToVector3();
    Vector3 rot = spherical_.ToRotatedVector3();
    if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(pos);
        tr->SetRotate(rot);
    }
}

} // namespace KashipanEngine
