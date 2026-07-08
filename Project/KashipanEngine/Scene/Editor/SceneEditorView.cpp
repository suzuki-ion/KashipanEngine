#include "SceneEditorView.h"
#ifdef USE_IMGUI
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <type_traits>
#include <variant>

#include "Scene/Editor/EditorSettings.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Scene/Components/SceneObjectCollider.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Collider/ICollider.h"
#include "Objects/Components/Collider/RayCollider.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Utilities/MathUtils.h"

namespace KashipanEngine {

SceneEditorView::SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context)
    : context_(context), gizmoOperation_(ImGuizmo::TRANSLATE), gizmoMode_(ImGuizmo::LOCAL) {
    // デバッグ表示の有効/無効を復元する（再起動後も維持される）
    showGrid_ = EditorSettings::GetBool("sceneView.showGrid", true);
    showLightMarkers_ = EditorSettings::GetBool("sceneView.showLightMarkers", true);
    showCameraMarkers_ = EditorSettings::GetBool("sceneView.showCameraMarkers", true);
    showColliderGizmos_ = EditorSettings::GetBool("sceneView.showColliderGizmos", true);
}

SceneEditorView::~SceneEditorView() {
    if (context_) {
        if (auto *sceneRenderer = context_->GetComponent<SceneRenderer>()) {
            sceneRenderer->SetEditorTarget(nullptr, nullptr);
        }
    }
    if (screenBuffer_ && ScreenBuffer::IsExist(screenBuffer_)) {
        screenBuffer_->DestroyNotify();
    }
    screenBuffer_ = nullptr;
}

void SceneEditorView::ShowImGui(EmptyObject *selectedObject, SceneEditorCommands *commands) {
    EnsureResources();
    UpdateCameraBuffer();
    RegisterEditorTarget();
    ShowSceneViewWindow(selectedObject, commands);
}

void SceneEditorView::EnsureResources() {
    if (!screenBuffer_ || !ScreenBuffer::IsExist(screenBuffer_)) {
        screenBuffer_ = ScreenBuffer::Create(1280, 720, "EditorSceneView");
    }
    if (!cameraBuffer_) {
        cameraBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CameraConstant));
    }
}

void SceneEditorView::UpdateCameraBuffer() {
    if (!cameraBuffer_ || !screenBuffer_) return;

    // ピッチ→ヨーの順で回転（行ベクトル規約）
    Matrix4x4 rotateX;
    rotateX.MakeRotateX(pitch_);
    Matrix4x4 rotateY;
    rotateY.MakeRotateY(yaw_);
    const Matrix4x4 rotation = rotateX * rotateY;

    const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);
    const Vector3 eye = target_ - forward * distance_;

    Matrix4x4 world = rotation;
    world.m[3][0] = eye.x;
    world.m[3][1] = eye.y;
    world.m[3][2] = eye.z;
    world.m[3][3] = 1.0f;

    view_ = world.Inverse();
    const float width = static_cast<float>(screenBuffer_->GetWidth());
    const float height = static_cast<float>(screenBuffer_->GetHeight());
    const float aspect = (height > 0.0f) ? (width / height) : (16.0f / 9.0f);
    projection_.MakePerspectiveFovMatrix(fovY_, aspect, nearClip_, farClip_);

    CameraConstant constant{};
    constant.view = view_;
    constant.projection = projection_;
    constant.viewProjection = view_ * projection_;
    constant.eyePosition = Vector4(eye.x, eye.y, eye.z, 1.0f);
    constant.fov = fovY_;

    if (void *mapped = cameraBuffer_->Map()) {
        std::memcpy(mapped, &constant, sizeof(constant));
    }
}

void SceneEditorView::RegisterEditorTarget() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (sceneRenderer && screenBuffer_) {
        sceneRenderer->SetEditorTarget(screenBuffer_, cameraBuffer_.get());
    }
}

void SceneEditorView::ShowSceneViewWindow(EmptyObject *selectedObject, SceneEditorCommands *commands) {
    if (!ImGui::Begin("Scene View")) {
        ImGui::End();
        return;
    }

    //--------- ギズモ操作の切り替えツールバー ---------//
    if (ImGui::RadioButton("Translate", gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Local", gizmoMode_ == ImGuizmo::LOCAL)) gizmoMode_ = ImGuizmo::LOCAL;
    ImGui::SameLine();
    if (ImGui::RadioButton("World", gizmoMode_ == ImGuizmo::WORLD)) gizmoMode_ = ImGuizmo::WORLD;

    //--------- デバッグ表示の有効/無効切り替え ---------//
    if (ImGui::Checkbox("Grid", &showGrid_)) EditorSettings::SetBool("sceneView.showGrid", showGrid_);
    ImGui::SameLine();
    if (ImGui::Checkbox("Lights", &showLightMarkers_)) EditorSettings::SetBool("sceneView.showLightMarkers", showLightMarkers_);
    ImGui::SameLine();
    if (ImGui::Checkbox("Cameras", &showCameraMarkers_)) EditorSettings::SetBool("sceneView.showCameraMarkers", showCameraMarkers_);
    ImGui::SameLine();
    if (ImGui::Checkbox("Colliders", &showColliderGizmos_)) EditorSettings::SetBool("sceneView.showColliderGizmos", showColliderGizmos_);

    //--------- シーンビュー画像 ---------//
    if (!screenBuffer_ || screenBuffer_->GetSrvHandle().ptr == 0) {
        ImGui::TextUnformatted("Scene view is not ready.");
        ImGui::End();
        return;
    }

    // ウィンドウの表示領域に合わせて ScreenBuffer 自体をリサイズする（Unity等のシーンビューと同じ挙動）
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x >= 1.0f && avail.y >= 1.0f) {
        const auto newWidth = static_cast<std::uint32_t>(avail.x);
        const auto newHeight = static_cast<std::uint32_t>(avail.y);
        if (newWidth != screenBuffer_->GetWidth() || newHeight != screenBuffer_->GetHeight()) {
            screenBuffer_->Resize(newWidth, newHeight);
        }
    }
    const ImVec2 drawSize = avail;
    const ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(screenBuffer_->GetSrvHandle().ptr), drawSize);

    //--------- カメラ操作（画像上でのマウス操作） ---------//
    HandleCameraInput();

    //--------- XZ平面のグリッド線 ---------//
    if (showGrid_) DrawGrid(imagePos, drawSize);

    //--------- ライトのデバッグ表示 ---------//
    if (showLightMarkers_) DrawLightMarkers(imagePos, drawSize);

    //--------- カメラのデバッグ表示 ---------//
    if (showCameraMarkers_) DrawCameraMarkers(imagePos, drawSize);

    //--------- 当たり判定のデバッグ表示 ---------//
    if (showColliderGizmos_) DrawColliderGizmos(imagePos, drawSize);

    //--------- ImGuizmo によるギズモ表示 ---------//
    ShowGizmo(selectedObject, commands, imagePos, drawSize);

    ImGui::End();
}

void SceneEditorView::HandleCameraInput() {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO &io = ImGui::GetIO();

    // ホイールでズーム
    if (io.MouseWheel != 0.0f) {
        distance_ *= std::pow(0.9f, io.MouseWheel);
        distance_ = std::clamp(distance_, 0.1f, 10000.0f);
    }
    // 右ドラッグで回転
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        yaw_ += io.MouseDelta.x * 0.005f;
        pitch_ += io.MouseDelta.y * 0.005f;
        pitch_ = std::clamp(pitch_, -1.55f, 1.55f);
    }
    // 中ドラッグでパン
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        Matrix4x4 rotateX;
        rotateX.MakeRotateX(pitch_);
        Matrix4x4 rotateY;
        rotateY.MakeRotateY(yaw_);
        const Matrix4x4 rotation = rotateX * rotateY;
        const Vector3 right(rotation.m[0][0], rotation.m[0][1], rotation.m[0][2]);
        const Vector3 up(rotation.m[1][0], rotation.m[1][1], rotation.m[1][2]);
        const float panSpeed = distance_ * 0.002f;
        target_ = target_ - right * (io.MouseDelta.x * panSpeed) + up * (io.MouseDelta.y * panSpeed);
    }
}

void SceneEditorView::DrawGrid(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

    Matrix4x4 view = view_;
    Matrix4x4 projection = projection_;
    Matrix4x4 identity = Matrix4x4::Identity();
    ImGuizmo::DrawGrid(&view.m[0][0], &projection.m[0][0], &identity.m[0][0], 100.0f);
}

bool SceneEditorView::ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos, bool clampToVisibleArea) const {
    const Matrix4x4 viewProjection = view_ * projection_;
    const float x = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float y = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    const float w = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (w <= 1e-4f) return false;
    const float ndcX = x / w;
    const float ndcY = y / w;
    if (clampToVisibleArea && (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f)) return false;
    outScreenPos = ImVec2(
        imagePos.x + (ndcX * 0.5f + 0.5f) * imageSize.x,
        imagePos.y + (-ndcY * 0.5f + 0.5f) * imageSize.y);
    return true;
}

void SceneEditorView::DrawLightMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    for (auto *lightRenderer : sceneRenderer->GetLightRenderers()) {
        if (!lightRenderer || !lightRenderer->IsActive()) continue;

        const Vector3 position = lightRenderer->GetWorldPosition();
        ImVec2 center;
        if (!ProjectToImage(position, imagePos, imageSize, center)) continue;

        // ライトの色をアイコン色にする（視認性のため不透明にする）
        ImU32 color = IM_COL32(255, 220, 96, 255);
        auto *light = lightRenderer->GetLight();
        if (light) {
            const Vector4 &lightColor = light->GetColor();
            color = IM_COL32(
                static_cast<int>(std::clamp(lightColor.x, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(lightColor.y, 0.0f, 1.0f) * 255.0f),
                static_cast<int>(std::clamp(lightColor.z, 0.0f, 1.0f) * 255.0f),
                255);
        }

        // 電球風アイコン（円＋放射線）
        constexpr float kRadius = 7.0f;
        drawList->AddCircle(center, kRadius, color, 12, 2.0f);
        drawList->AddCircleFilled(center, 2.5f, color);
        const auto lightType = light ? light->GetType() : Light::Type::Directional;
        if (lightType != Light::Type::Spot) {
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * (std::numbers::pi_v<float> / 4.0f);
                const float dirX = std::cos(angle);
                const float dirY = std::sin(angle);
                drawList->AddLine(
                    ImVec2(center.x + dirX * (kRadius + 2.0f), center.y + dirY * (kRadius + 2.0f)),
                    ImVec2(center.x + dirX * (kRadius + 6.0f), center.y + dirY * (kRadius + 6.0f)),
                    color, 1.5f);
            }
        }

        // 方向を持つライトは向きを線で表示する
        if (lightType == Light::Type::Directional || lightType == Light::Type::Spot) {
            const Vector3 direction = lightRenderer->GetWorldDirection();
            const float length = (lightType == Light::Type::Spot && light) ? light->GetDistance() : 2.0f;
            ImVec2 tip;
            if (ProjectToImage(position + direction * length, imagePos, imageSize, tip)) {
                drawList->AddLine(center, tip, color, 2.0f);
                drawList->AddCircleFilled(tip, 3.0f, color);
            }
        }
    }

    drawList->PopClipRect();
}

namespace {
/// @brief NDC座標をワールド座標へ逆投影する（行ベクトル規約、D3Dの深度レンジ[0,1]）
Vector3 UnprojectNdc(const Matrix4x4 &invViewProjection, float ndcX, float ndcY, float ndcZ) {
    const float x = ndcX * invViewProjection.m[0][0] + ndcY * invViewProjection.m[1][0] + ndcZ * invViewProjection.m[2][0] + invViewProjection.m[3][0];
    const float y = ndcX * invViewProjection.m[0][1] + ndcY * invViewProjection.m[1][1] + ndcZ * invViewProjection.m[2][1] + invViewProjection.m[3][1];
    const float z = ndcX * invViewProjection.m[0][2] + ndcY * invViewProjection.m[1][2] + ndcZ * invViewProjection.m[2][2] + invViewProjection.m[3][2];
    float w = ndcX * invViewProjection.m[0][3] + ndcY * invViewProjection.m[1][3] + ndcZ * invViewProjection.m[2][3] + invViewProjection.m[3][3];
    if (std::abs(w) < 1e-6f) w = 1e-6f;
    return Vector3(x / w, y / w, z / w);
}
} // namespace

void SceneEditorView::DrawCameraMarkers(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;

        const Vector3 position = cameraRenderer->GetWorldPosition();
        ImVec2 center;
        if (!ProjectToImage(position, imagePos, imageSize, center)) continue;

        constexpr ImU32 color = IM_COL32(120, 200, 255, 255);

        // カメラ風アイコン（本体の四角＋レンズの円）
        constexpr float kHalfWidth = 8.0f;
        constexpr float kHalfHeight = 6.0f;
        drawList->AddRect(
            ImVec2(center.x - kHalfWidth, center.y - kHalfHeight),
            ImVec2(center.x + kHalfWidth, center.y + kHalfHeight),
            color, 2.0f, 0, 2.0f);
        drawList->AddCircle(center, 3.5f, color, 12, 1.5f);

        // 視錐台
        DrawCameraFrustum(cameraRenderer, imagePos, imageSize, color);
    }

    drawList->PopClipRect();
}

void SceneEditorView::DrawCameraFrustum(CameraRenderer *cameraRenderer, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    if (!cameraRenderer) return;

    const Matrix4x4 invViewProjection = cameraRenderer->GetViewProjectionMatrix().Inverse();

    // NDCの4隅（近平面 z=0 / 遠平面 z=1、D3Dの深度レンジ）をワールド座標へ逆投影する
    static constexpr float kNdcXY[4][2] = { {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f} };
    Vector3 nearCorners[4];
    Vector3 farCorners[4];
    for (int i = 0; i < 4; ++i) {
        nearCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 0.0f);
        farCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 1.0f);
    }

    ImVec2 nearScreen[4];
    ImVec2 farScreen[4];
    bool nearValid[4];
    bool farValid[4];
    for (int i = 0; i < 4; ++i) {
        // 頂点がウィンドウ外にはみ出していても、線自体はImGuiのクリップ矩形で正しく切り取られるように
        // クランプせずに射影する（カメラの背後にある場合のみ false になる）
        nearValid[i] = ProjectToImage(nearCorners[i], imagePos, imageSize, nearScreen[i], false);
        farValid[i] = ProjectToImage(farCorners[i], imagePos, imageSize, farScreen[i], false);
    }

    auto *drawList = ImGui::GetWindowDrawList();
    for (int i = 0; i < 4; ++i) {
        const int next = (i + 1) % 4;
        if (nearValid[i] && nearValid[next]) drawList->AddLine(nearScreen[i], nearScreen[next], color, 1.5f);
        if (farValid[i] && farValid[next]) drawList->AddLine(farScreen[i], farScreen[next], color, 1.0f);
        if (nearValid[i] && farValid[i]) drawList->AddLine(nearScreen[i], farScreen[i], color, 1.0f);
    }
}

void SceneEditorView::DrawColliderGizmos(const ImVec2 &imagePos, const ImVec2 &imageSize) {
    if (!context_ || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *sceneObjectCollider = context_->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) return;

    constexpr ImU32 kSolidColor = IM_COL32(80, 230, 120, 255);
    constexpr ImU32 kTriggerColor = IM_COL32(255, 200, 40, 255);
    constexpr ImU32 kRayColor = IM_COL32(255, 90, 220, 255);

    auto *drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y), true);

    for (auto *collider : sceneObjectCollider->GetRegisteredColliders()) {
        if (!collider || !collider->IsActive()) continue;
        const ImU32 color = collider->IsTrigger() ? kTriggerColor : kSolidColor;

        if (collider->Is2D()) {
            if (auto info = collider->BuildColliderInfo2D()) {
                DrawCollider2DShape(*info, collider->GetOwnerWorldPosition().z, imagePos, imageSize, color);
            }
            continue;
        }

        if (auto info = collider->BuildColliderInfo3D()) {
            std::visit([&](const auto &shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, ColliderInfo3D::SphereShape3D>) {
                    DrawWireSphere3D(shape.center, shape.radius, imagePos, imageSize, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::BoxShape3D>) {
                    DrawWireBox3D(shape.center, shape.halfExtents, imagePos, imageSize, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::CapsuleShape3D>) {
                    DrawWireCapsule3D(shape.center, shape.radius, shape.height, imagePos, imageSize, color);
                }
                // ConvexMesh/ConcaveMesh/HeightFieldはエンジン内に生成コンポーネントが無いため未対応
            }, info->shape);
        } else if (auto *rayCollider = dynamic_cast<RayCollider *>(collider)) {
            // RayColliderは常駐形状を持たないため、方向・距離からレイとして描画する
            DrawRayGizmo(collider->GetOwnerWorldPosition(), rayCollider->GetDirection(), rayCollider->GetMaxDistance(), imagePos, imageSize, kRayColor);
        }
    }

    drawList->PopClipRect();
}

void SceneEditorView::DrawWireBox3D(const Vector3 &center, const Vector3 &halfExtents, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    Vector3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i] = Vector3(
            center.x + ((i & 1) ? halfExtents.x : -halfExtents.x),
            center.y + ((i & 2) ? halfExtents.y : -halfExtents.y),
            center.z + ((i & 4) ? halfExtents.z : -halfExtents.z));
    }
    ImVec2 screen[8];
    bool valid[8];
    for (int i = 0; i < 8; ++i) {
        valid[i] = ProjectToImage(corners[i], imagePos, imageSize, screen[i], false);
    }

    static constexpr int kEdges[12][2] = {
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
        {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7},
    };
    auto *drawList = ImGui::GetWindowDrawList();
    for (const auto &edge : kEdges) {
        if (valid[edge[0]] && valid[edge[1]]) {
            drawList->AddLine(screen[edge[0]], screen[edge[1]], color, 1.5f);
        }
    }
}

void SceneEditorView::DrawWireSphere3D(const Vector3 &center, float radius, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    constexpr int kSegments = 24;
    auto *drawList = ImGui::GetWindowDrawList();

    // XY・XZ・YZの3つの円で球を近似する
    for (int plane = 0; plane < 3; ++plane) {
        ImVec2 prevScreen{};
        bool prevValid = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            const float c = std::cos(angle) * radius;
            const float s = std::sin(angle) * radius;
            Vector3 point = center;
            if (plane == 0) { point.x += c; point.y += s; }
            else if (plane == 1) { point.x += c; point.z += s; }
            else { point.y += c; point.z += s; }

            ImVec2 screen;
            const bool valid = ProjectToImage(point, imagePos, imageSize, screen, false);
            if (valid && prevValid) drawList->AddLine(prevScreen, screen, color, 1.5f);
            prevScreen = screen;
            prevValid = valid;
        }
    }
}

void SceneEditorView::DrawWireCapsule3D(const Vector3 &center, float radius, float height, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    constexpr int kSegments = 24;
    auto *drawList = ImGui::GetWindowDrawList();
    const float halfHeight = height * 0.5f;
    const Vector3 topCenter = center + Vector3(0.0f, halfHeight, 0.0f);
    const Vector3 bottomCenter = center - Vector3(0.0f, halfHeight, 0.0f);

    // カプセルはReactPhysics3Dの規約に合わせてY軸方向を軸とする（半球部は簡略化して円柱部のみ描画する）
    auto drawCircleXZ = [&](const Vector3 &circleCenter) {
        ImVec2 prevScreen{};
        bool prevValid = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            Vector3 point = circleCenter;
            point.x += std::cos(angle) * radius;
            point.z += std::sin(angle) * radius;
            ImVec2 screen;
            const bool valid = ProjectToImage(point, imagePos, imageSize, screen, false);
            if (valid && prevValid) drawList->AddLine(prevScreen, screen, color, 1.5f);
            prevScreen = screen;
            prevValid = valid;
        }
    };
    drawCircleXZ(topCenter);
    drawCircleXZ(bottomCenter);

    for (int i = 0; i < 4; ++i) {
        const float angle = static_cast<float>(i) / 4.0f * 2.0f * std::numbers::pi_v<float>;
        const float dx = std::cos(angle) * radius;
        const float dz = std::sin(angle) * radius;
        ImVec2 topScreen;
        ImVec2 bottomScreen;
        const bool topValid = ProjectToImage(topCenter + Vector3(dx, 0.0f, dz), imagePos, imageSize, topScreen, false);
        const bool bottomValid = ProjectToImage(bottomCenter + Vector3(dx, 0.0f, dz), imagePos, imageSize, bottomScreen, false);
        if (topValid && bottomValid) drawList->AddLine(topScreen, bottomScreen, color, 1.5f);
    }
}

void SceneEditorView::DrawCollider2DShape(const ColliderInfo2D &info, float worldZ, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    auto *drawList = ImGui::GetWindowDrawList();

    std::visit([&](const auto &shape) {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, Math::Point2D>) {
            ImVec2 screen;
            if (ProjectToImage(Vector3(shape.position.x, shape.position.y, worldZ), imagePos, imageSize, screen, false)) {
                drawList->AddCircleFilled(screen, 3.0f, color);
            }
        } else if constexpr (std::is_same_v<T, Math::Circle>) {
            constexpr int kSegments = 24;
            ImVec2 prevScreen{};
            bool prevValid = false;
            for (int i = 0; i <= kSegments; ++i) {
                const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
                const Vector3 point(shape.center.x + std::cos(angle) * shape.radius, shape.center.y + std::sin(angle) * shape.radius, worldZ);
                ImVec2 screen;
                const bool valid = ProjectToImage(point, imagePos, imageSize, screen, false);
                if (valid && prevValid) drawList->AddLine(prevScreen, screen, color, 1.5f);
                prevScreen = screen;
                prevValid = valid;
            }
        } else if constexpr (std::is_same_v<T, Math::Rect>) {
            const Vector2 corners2D[4] = {
                Vector2(shape.center.x - shape.halfSize.x, shape.center.y - shape.halfSize.y),
                Vector2(shape.center.x + shape.halfSize.x, shape.center.y - shape.halfSize.y),
                Vector2(shape.center.x + shape.halfSize.x, shape.center.y + shape.halfSize.y),
                Vector2(shape.center.x - shape.halfSize.x, shape.center.y + shape.halfSize.y),
            };
            ImVec2 screen[4];
            bool valid[4];
            for (int i = 0; i < 4; ++i) {
                valid[i] = ProjectToImage(Vector3(corners2D[i].x, corners2D[i].y, worldZ), imagePos, imageSize, screen[i], false);
            }
            for (int i = 0; i < 4; ++i) {
                const int next = (i + 1) % 4;
                if (valid[i] && valid[next]) drawList->AddLine(screen[i], screen[next], color, 1.5f);
            }
        } else if constexpr (std::is_same_v<T, Math::Segment2D>) {
            ImVec2 startScreen;
            ImVec2 endScreen;
            const bool startValid = ProjectToImage(Vector3(shape.start.x, shape.start.y, worldZ), imagePos, imageSize, startScreen, false);
            const bool endValid = ProjectToImage(Vector3(shape.end.x, shape.end.y, worldZ), imagePos, imageSize, endScreen, false);
            if (startValid && endValid) drawList->AddLine(startScreen, endScreen, color, 2.0f);
            if (endValid) drawList->AddCircleFilled(endScreen, 3.0f, color);
        } else if constexpr (std::is_same_v<T, Math::Capsule2D>) {
            const Vector2 axis = shape.end - shape.start;
            const float axisLength = axis.Length();
            const Vector2 dir = axisLength > 1e-6f ? axis * (1.0f / axisLength) : Vector2(1.0f, 0.0f);
            const Vector2 offset = dir.Perpendicular() * shape.radius;

            ImVec2 s1;
            ImVec2 s2;
            ImVec2 e1;
            ImVec2 e2;
            const bool s1Valid = ProjectToImage(Vector3(shape.start.x + offset.x, shape.start.y + offset.y, worldZ), imagePos, imageSize, s1, false);
            const bool s2Valid = ProjectToImage(Vector3(shape.start.x - offset.x, shape.start.y - offset.y, worldZ), imagePos, imageSize, s2, false);
            const bool e1Valid = ProjectToImage(Vector3(shape.end.x + offset.x, shape.end.y + offset.y, worldZ), imagePos, imageSize, e1, false);
            const bool e2Valid = ProjectToImage(Vector3(shape.end.x - offset.x, shape.end.y - offset.y, worldZ), imagePos, imageSize, e2, false);
            if (s1Valid && e1Valid) drawList->AddLine(s1, e1, color, 1.5f);
            if (s2Valid && e2Valid) drawList->AddLine(s2, e2, color, 1.5f);

            // 端の半円（近似）
            const float baseAngle = std::atan2(dir.y, dir.x);
            auto drawArc = [&](const Vector2 &arcCenter, float startAngle) {
                constexpr int kArcSegments = 12;
                ImVec2 prevScreen{};
                bool prevValid = false;
                for (int i = 0; i <= kArcSegments; ++i) {
                    const float angle = startAngle + static_cast<float>(i) / static_cast<float>(kArcSegments) * std::numbers::pi_v<float>;
                    const Vector3 point(arcCenter.x + std::cos(angle) * shape.radius, arcCenter.y + std::sin(angle) * shape.radius, worldZ);
                    ImVec2 screen;
                    const bool valid = ProjectToImage(point, imagePos, imageSize, screen, false);
                    if (valid && prevValid) drawList->AddLine(prevScreen, screen, color, 1.5f);
                    prevScreen = screen;
                    prevValid = valid;
                }
            };
            drawArc(shape.end, baseAngle - std::numbers::pi_v<float> * 0.5f);
            drawArc(shape.start, baseAngle + std::numbers::pi_v<float> * 0.5f);
        }
    }, info.shape);
}

void SceneEditorView::DrawRayGizmo(const Vector3 &origin, const Vector3 &direction, float length, const ImVec2 &imagePos, const ImVec2 &imageSize, ImU32 color) {
    const Vector3 dir = direction.Length() > 1e-6f ? direction.Normalize() : Vector3(0.0f, -1.0f, 0.0f);
    const Vector3 end = origin + dir * length;

    ImVec2 startScreen;
    ImVec2 endScreen;
    const bool startValid = ProjectToImage(origin, imagePos, imageSize, startScreen, false);
    const bool endValid = ProjectToImage(end, imagePos, imageSize, endScreen, false);

    auto *drawList = ImGui::GetWindowDrawList();
    if (startValid && endValid) drawList->AddLine(startScreen, endScreen, color, 2.0f);
    if (endValid) drawList->AddCircleFilled(endScreen, 3.0f, color);
}

void SceneEditorView::ShowGizmo(EmptyObject *selectedObject, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    // 選択オブジェクトが変わった場合は編集状態をリセットする
    if (selectedObject != gizmoTargetObject_) {
        gizmoTargetObject_ = selectedObject;
        isGizmoEditing_ = false;
    }
    if (!selectedObject || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;
    auto *transform = selectedObject->GetComponent<Transform>();
    if (!transform) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

    Matrix4x4 model = transform->GetWorldMatrix();
    Matrix4x4 view = view_;
    Matrix4x4 projection = projection_;

    ImGuizmo::Manipulate(
        &view.m[0][0], &projection.m[0][0],
        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
        static_cast<ImGuizmo::MODE>(gizmoMode_),
        &model.m[0][0]);

    if (ImGuizmo::IsUsing()) {
        // 操作開始時に変更前の状態を保存する（Undo用）
        if (!isGizmoEditing_) {
            isGizmoEditing_ = true;
            gizmoBefore_ = selectedObject->SaveComponentToJson(transform);
        }

        // 親がいる場合はローカル行列へ変換してから適用する
        Matrix4x4 local = model;
        if (auto *parentObject = transform->GetParentObject()) {
            if (auto *parentTransform = parentObject->GetComponent<Transform>()) {
                local = model * parentTransform->GetWorldMatrix().Inverse();
            }
        }

        Vector3 translate;
        Vector3 rotateDeg;
        Vector3 scale;
        ImGuizmo::DecomposeMatrixToComponents(&local.m[0][0], &translate.x, &rotateDeg.x, &scale.x);

        // オイラー角を経由すると（ジンバルロックや ImGuizmo との回転順の差により）
        // ドラッグ中に回転が不安定になるため、回転はローカル行列から直接クォータニオンへ変換する。
        // （translate/scale の抽出には ImGuizmo の分解結果をそのまま使う）
        Matrix4x4 rotationOnly = local;
        auto normalizeRow = [](Matrix4x4 &m, int row, float scaleValue) {
            if (std::abs(scaleValue) < 1e-8f) return;
            m.m[row][0] /= scaleValue;
            m.m[row][1] /= scaleValue;
            m.m[row][2] /= scaleValue;
        };
        normalizeRow(rotationOnly, 0, scale.x);
        normalizeRow(rotationOnly, 1, scale.y);
        normalizeRow(rotationOnly, 2, scale.z);

        transform->SetTranslate(translate);
        transform->SetRotateQuaternion(Quaternion::MakeFromRotationMatrix(rotationOnly));
        transform->SetScale(scale);
    } else if (isGizmoEditing_) {
        // 操作終了時にUndo/Redo履歴へ積む
        isGizmoEditing_ = false;
        JSON after = selectedObject->SaveComponentToJson(transform);
        if (commands && after != gizmoBefore_) {
            commands->PushExecuted(std::make_unique<ComponentEditCommand>(selectedObject, transform, gizmoBefore_, after));
        }
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
