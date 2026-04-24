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

DebugCameraMovement::DebugCameraMovement(Camera3D* camera)
    : ISceneComponent("DebugCameraMovement", 1), camera_(camera) {
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
    if (!input_ && ownerContext) {
        input_ = ownerContext->GetInput();
    }
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
    if (auto *tr = camera_->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(pos);

        // カメラの前方方向を計算（注視点へ向かうベクトル）
        Vector3 forward = (spherical_.origin - pos);
        float len = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
        if (len > 0.0001f) {
            forward = forward * (1.0f / len);
        }

        // forward と worldUp から回転行列を構築し、クォータニオンで設定
        Vector3 worldUp(0.0f, 1.0f, 0.0f);

        // right = worldUp × forward（左手系）
        Vector3 right = worldUp.Cross(forward);
        float rightLen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
        if (rightLen > 0.0001f) {
            right = right * (1.0f / rightLen);
        }

        // up = forward × right
        Vector3 up = forward.Cross(right);
        float upLen = std::sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
        if (upLen > 0.0001f) {
            up = up * (1.0f / upLen);
        }

        // 回転行列からクォータニオンを構築
        // 行列: row0=right, row1=up, row2=forward（エンジンの行ベクトル形式に合わせる）
        float m00 = right.x,   m01 = right.y,   m02 = right.z;
        float m10 = up.x,      m11 = up.y,      m12 = up.z;
        float m20 = forward.x, m21 = forward.y,  m22 = forward.z;

        float trace = m00 + m11 + m22;
        Quaternion q;
        if (trace > 0.0f) {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m12 - m21) / s;
            q.y = (m20 - m02) / s;
            q.z = (m01 - m10) / s;
        } else if (m00 > m11 && m00 > m22) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m12 - m21) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m20 + m02) / s;
        } else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m20 - m02) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m01 - m10) / s;
            q.x = (m20 + m02) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }

        tr->SetRotateQuaternion(q.Normalize());
    }
}

} // namespace KashipanEngine
