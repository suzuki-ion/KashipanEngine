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
#include "Assets/TextureManager.h"
#include "Math/Quaternion.h"
#include "Utilities/MathUtils.h"
#include "Utilities/AssetDragDropPayload.h"

namespace KashipanEngine {

SceneEditorView::SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context)
    : context_(context), gizmoOperation_(ImGuizmo::TRANSLATE), gizmoMode_(ImGuizmo::LOCAL) {
    // デバッグ表示の有効/無効を復元する（再起動後も維持される）
    showGrid_ = EditorSettings::GetBool("sceneView.showGrid", true);
    showLightMarkers_ = EditorSettings::GetBool("sceneView.showLightMarkers", true);
    showCameraMarkers_ = EditorSettings::GetBool("sceneView.showCameraMarkers", true);
    showColliderGizmos_ = EditorSettings::GetBool("sceneView.showColliderGizmos", true);

    // 背景設定を復元する（再起動後も維持される）
    backgroundColor_.x = EditorSettings::GetFloat("sceneView.backgroundColor.r", 0.0f);
    backgroundColor_.y = EditorSettings::GetFloat("sceneView.backgroundColor.g", 0.0f);
    backgroundColor_.z = EditorSettings::GetFloat("sceneView.backgroundColor.b", 0.0f);
    backgroundColor_.w = EditorSettings::GetFloat("sceneView.backgroundColor.a", 1.0f);
    backgroundTexturePath_ = EditorSettings::GetString("sceneView.backgroundTexturePath", std::string{});
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

void SceneEditorView::ShowImGui(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands) {
    EnsureResources();
    UpdateCameraBuffer();
    RegisterEditorTarget();
    UpdateEditorDebugDraw();
    ShowSceneViewWindow(selectedObjects, commands);
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

void SceneEditorView::ShowSceneViewWindow(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands) {
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

    //--------- 背景設定（単色 or テクスチャ） ---------//
    if (ImGui::ColorEdit4("Background Color", &backgroundColor_.x)) {
        EditorSettings::SetFloat("sceneView.backgroundColor.r", backgroundColor_.x);
        EditorSettings::SetFloat("sceneView.backgroundColor.g", backgroundColor_.y);
        EditorSettings::SetFloat("sceneView.backgroundColor.b", backgroundColor_.z);
        EditorSettings::SetFloat("sceneView.backgroundColor.a", backgroundColor_.w);
    }
    ImGui::SameLine();
    std::vector<std::string> texturePaths;
    for (const auto &entry : TextureManager::GetLoadedTextureListEntries()) {
        texturePaths.push_back(entry.assetPath);
    }
    if (ImGuiCustom::SelectString("Background Texture", backgroundTexturePath_, texturePaths, true)) {
        EditorSettings::SetString("sceneView.backgroundTexturePath", backgroundTexturePath_);
    }
    // Assetsウィンドウからのテクスチャファイルドラッグ&ドロップも受け付ける
    if (std::string droppedPath; AcceptAssetDragDropTarget(kTextureAssetDragDropType, droppedPath)) {
        backgroundTexturePath_ = droppedPath;
        EditorSettings::SetString("sceneView.backgroundTexturePath", backgroundTexturePath_);
    }

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

    // グリッド線・当たり判定のワイヤーフレームは screenBuffer_ へGPUで直接描画される
    // （UpdateEditorDebugDraw で設定済み。DebugGrid/DebugLinesパイプライン参照）

    //--------- ライトのデバッグ表示 ---------//
    if (showLightMarkers_) DrawLightMarkers(imagePos, drawSize);

    //--------- カメラのデバッグ表示 ---------//
    if (showCameraMarkers_) DrawCameraMarkers(imagePos, drawSize);

    //--------- ImGuizmo によるギズモ表示 ---------//
    ShowGizmo(selectedObjects, commands, imagePos, drawSize);

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

void SceneEditorView::UpdateEditorDebugDraw() {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    EditorDebugDrawSettings settings;
    settings.showGrid = showGrid_;
    settings.showColliderGizmos = showColliderGizmos_;
    // カメラのズーム量に応じてグリッドの表示範囲を追従させる（Blenderのように「無限」に感じられる範囲を保つ）
    settings.gridFadeDistance = std::clamp(distance_ * 4.0f, 20.0f, 2000.0f);

    settings.backgroundColor = backgroundColor_;
    settings.backgroundTextureHandle = backgroundTexturePath_.empty()
        ? TextureManager::kInvalidHandle
        : TextureManager::GetTextureFromAssetPath(backgroundTexturePath_);

    if (showColliderGizmos_) {
        AppendColliderDebugLines(settings.lines);
    }
    if (showCameraMarkers_) {
        AppendCameraFrustumLines(settings.lines);
    }

    sceneRenderer->SetEditorDebugDraw(std::move(settings));
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
    }

    drawList->PopClipRect();
}

void SceneEditorView::AppendCameraFrustumLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
    if (!sceneRenderer) return;

    constexpr Vector4 kNearColor{ 0.47f, 0.78f, 1.0f, 1.0f };
    constexpr Vector4 kFarColor{ 0.47f, 0.78f, 1.0f, 0.6f };

    for (auto *cameraRenderer : sceneRenderer->GetCameraRenderers()) {
        if (!cameraRenderer || !cameraRenderer->IsActive()) continue;

        const Matrix4x4 invViewProjection = cameraRenderer->GetViewProjectionMatrix().Inverse();

        // NDCの4隅（近平面 z=0 / 遠平面 z=1、D3Dの深度レンジ）をワールド座標へ逆投影する
        static constexpr float kNdcXY[4][2] = { {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f} };
        Vector3 nearCorners[4];
        Vector3 farCorners[4];
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 0.0f);
            farCorners[i] = UnprojectNdc(invViewProjection, kNdcXY[i][0], kNdcXY[i][1], 1.0f);
        }

        for (int i = 0; i < 4; ++i) {
            const int next = (i + 1) % 4;
            out.push_back({ nearCorners[i], kNearColor });
            out.push_back({ nearCorners[next], kNearColor });
            out.push_back({ farCorners[i], kFarColor });
            out.push_back({ farCorners[next], kFarColor });
            out.push_back({ nearCorners[i], kNearColor });
            out.push_back({ farCorners[i], kFarColor });
        }
    }
}

void SceneEditorView::AppendColliderDebugLines(std::vector<DebugLineVertex> &out) {
    if (!context_) return;
    auto *sceneObjectCollider = context_->GetComponent<SceneObjectCollider>();
    if (!sceneObjectCollider) return;

    constexpr Vector4 kSolidColor{ 0.31f, 0.90f, 0.47f, 1.0f };
    constexpr Vector4 kTriggerColor{ 1.0f, 0.78f, 0.16f, 1.0f };
    constexpr Vector4 kRayColor{ 1.0f, 0.35f, 0.86f, 1.0f };

    for (auto *collider : sceneObjectCollider->GetRegisteredColliders()) {
        if (!collider || !collider->IsActive()) continue;
        const Vector4 &color = collider->IsTrigger() ? kTriggerColor : kSolidColor;

        if (collider->Is2D()) {
            if (auto info = collider->BuildColliderInfo2D()) {
                AppendCollider2DShape(out, *info, collider->GetOwnerWorldPosition().z, color);
            }
            continue;
        }

        if (auto info = collider->BuildColliderInfo3D()) {
            const Quaternion rotation = collider->GetSyncedOwnerRotation();
            std::visit([&](const auto &shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, ColliderInfo3D::SphereShape3D>) {
                    AppendWireSphere3D(out, shape.center, shape.radius, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::BoxShape3D>) {
                    AppendWireBox3D(out, shape.center, shape.halfExtents, rotation, color);
                } else if constexpr (std::is_same_v<T, ColliderInfo3D::CapsuleShape3D>) {
                    AppendWireCapsule3D(out, shape.center, shape.radius, shape.height, rotation, color);
                }
                // ConvexMesh/ConcaveMesh/HeightFieldはエンジン内に生成コンポーネントが無いため未対応
            }, info->shape);
        } else if (auto *rayCollider = dynamic_cast<RayCollider *>(collider)) {
            // RayColliderは常駐形状を持たないため、方向・距離からレイとして描画する
            const Vector3 rotatedDirection = collider->GetSyncedOwnerRotation().RotateVector(rayCollider->GetDirection());
            AppendRayGizmo(out, collider->GetSyncedOwnerPosition(), rotatedDirection, rayCollider->GetMaxDistance(), kRayColor);
        }
    }
}

void SceneEditorView::AppendWireBox3D(std::vector<DebugLineVertex> &out, const Vector3 &center, const Vector3 &halfExtents, const Quaternion &rotation, const Vector4 &color) {
    Vector3 corners[8];
    for (int i = 0; i < 8; ++i) {
        const Vector3 localOffset(
            (i & 1) ? halfExtents.x : -halfExtents.x,
            (i & 2) ? halfExtents.y : -halfExtents.y,
            (i & 4) ? halfExtents.z : -halfExtents.z);
        corners[i] = center + rotation.RotateVector(localOffset);
    }

    static constexpr int kEdges[12][2] = {
        {0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
        {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7},
    };
    for (const auto &edge : kEdges) {
        out.push_back({ corners[edge[0]], color });
        out.push_back({ corners[edge[1]], color });
    }
}

void SceneEditorView::AppendWireSphere3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, const Vector4 &color) {
    constexpr int kSegments = 24;

    // XY・XZ・YZの3つの円で球を近似する
    for (int plane = 0; plane < 3; ++plane) {
        Vector3 prevPoint{};
        bool hasPrev = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            const float c = std::cos(angle) * radius;
            const float s = std::sin(angle) * radius;
            Vector3 point = center;
            if (plane == 0) { point.x += c; point.y += s; }
            else if (plane == 1) { point.x += c; point.z += s; }
            else { point.y += c; point.z += s; }

            if (hasPrev) {
                out.push_back({ prevPoint, color });
                out.push_back({ point, color });
            }
            prevPoint = point;
            hasPrev = true;
        }
    }
}

void SceneEditorView::AppendWireCapsule3D(std::vector<DebugLineVertex> &out, const Vector3 &center, float radius, float height, const Quaternion &rotation, const Vector4 &color) {
    constexpr int kSegments = 24;
    const float halfHeight = height * 0.5f;
    const Vector3 topCenter = center + rotation.RotateVector(Vector3(0.0f, halfHeight, 0.0f));
    const Vector3 bottomCenter = center - rotation.RotateVector(Vector3(0.0f, halfHeight, 0.0f));

    // カプセルはReactPhysics3Dの規約に合わせてY軸方向を軸とする（半球部は簡略化して円柱部のみ描画する）
    // 円周上の点はローカルXZ平面で計算してからコライダーの回転を適用する
    auto appendCircleXZ = [&](const Vector3 &circleCenter) {
        Vector3 prevPoint{};
        bool hasPrev = false;
        for (int i = 0; i <= kSegments; ++i) {
            const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
            const Vector3 localOffset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
            const Vector3 point = circleCenter + rotation.RotateVector(localOffset);
            if (hasPrev) {
                out.push_back({ prevPoint, color });
                out.push_back({ point, color });
            }
            prevPoint = point;
            hasPrev = true;
        }
    };
    appendCircleXZ(topCenter);
    appendCircleXZ(bottomCenter);

    for (int i = 0; i < 4; ++i) {
        const float angle = static_cast<float>(i) / 4.0f * 2.0f * std::numbers::pi_v<float>;
        const Vector3 localOffset(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        const Vector3 rotatedOffset = rotation.RotateVector(localOffset);
        out.push_back({ topCenter + rotatedOffset, color });
        out.push_back({ bottomCenter + rotatedOffset, color });
    }
}

void SceneEditorView::AppendCollider2DShape(std::vector<DebugLineVertex> &out, const ColliderInfo2D &info, float worldZ, const Vector4 &color) {
    std::visit([&](const auto &shape) {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, Math::Point2D>) {
            // 点は微小な十字線として表現する
            const Vector3 p(shape.position.x, shape.position.y, worldZ);
            constexpr float kSize = 0.1f;
            out.push_back({ p - Vector3(kSize, 0.0f, 0.0f), color });
            out.push_back({ p + Vector3(kSize, 0.0f, 0.0f), color });
            out.push_back({ p - Vector3(0.0f, kSize, 0.0f), color });
            out.push_back({ p + Vector3(0.0f, kSize, 0.0f), color });
        } else if constexpr (std::is_same_v<T, Math::Circle>) {
            constexpr int kSegments = 24;
            Vector3 prevPoint{};
            bool hasPrev = false;
            for (int i = 0; i <= kSegments; ++i) {
                const float angle = static_cast<float>(i) / static_cast<float>(kSegments) * 2.0f * std::numbers::pi_v<float>;
                const Vector3 point(shape.center.x + std::cos(angle) * shape.radius, shape.center.y + std::sin(angle) * shape.radius, worldZ);
                if (hasPrev) {
                    out.push_back({ prevPoint, color });
                    out.push_back({ point, color });
                }
                prevPoint = point;
                hasPrev = true;
            }
        } else if constexpr (std::is_same_v<T, Math::Rect>) {
            const Vector3 corners[4] = {
                Vector3(shape.center.x - shape.halfSize.x, shape.center.y - shape.halfSize.y, worldZ),
                Vector3(shape.center.x + shape.halfSize.x, shape.center.y - shape.halfSize.y, worldZ),
                Vector3(shape.center.x + shape.halfSize.x, shape.center.y + shape.halfSize.y, worldZ),
                Vector3(shape.center.x - shape.halfSize.x, shape.center.y + shape.halfSize.y, worldZ),
            };
            for (int i = 0; i < 4; ++i) {
                const int next = (i + 1) % 4;
                out.push_back({ corners[i], color });
                out.push_back({ corners[next], color });
            }
        } else if constexpr (std::is_same_v<T, Math::Segment2D>) {
            out.push_back({ Vector3(shape.start.x, shape.start.y, worldZ), color });
            out.push_back({ Vector3(shape.end.x, shape.end.y, worldZ), color });
        } else if constexpr (std::is_same_v<T, Math::Capsule2D>) {
            const Vector2 axis = shape.end - shape.start;
            const float axisLength = axis.Length();
            const Vector2 dir = axisLength > 1e-6f ? axis * (1.0f / axisLength) : Vector2(1.0f, 0.0f);
            const Vector2 offset = dir.Perpendicular() * shape.radius;

            const Vector3 s1(shape.start.x + offset.x, shape.start.y + offset.y, worldZ);
            const Vector3 s2(shape.start.x - offset.x, shape.start.y - offset.y, worldZ);
            const Vector3 e1(shape.end.x + offset.x, shape.end.y + offset.y, worldZ);
            const Vector3 e2(shape.end.x - offset.x, shape.end.y - offset.y, worldZ);
            out.push_back({ s1, color });
            out.push_back({ e1, color });
            out.push_back({ s2, color });
            out.push_back({ e2, color });

            // 端の半円（近似）
            const float baseAngle = std::atan2(dir.y, dir.x);
            auto appendArc = [&](const Vector2 &arcCenter, float startAngle) {
                constexpr int kArcSegments = 12;
                Vector3 prevPoint{};
                bool hasPrev = false;
                for (int i = 0; i <= kArcSegments; ++i) {
                    const float angle = startAngle + static_cast<float>(i) / static_cast<float>(kArcSegments) * std::numbers::pi_v<float>;
                    const Vector3 point(arcCenter.x + std::cos(angle) * shape.radius, arcCenter.y + std::sin(angle) * shape.radius, worldZ);
                    if (hasPrev) {
                        out.push_back({ prevPoint, color });
                        out.push_back({ point, color });
                    }
                    prevPoint = point;
                    hasPrev = true;
                }
            };
            appendArc(shape.end, baseAngle - std::numbers::pi_v<float> * 0.5f);
            appendArc(shape.start, baseAngle + std::numbers::pi_v<float> * 0.5f);
        }
    }, info.shape);
}

void SceneEditorView::AppendRayGizmo(std::vector<DebugLineVertex> &out, const Vector3 &origin, const Vector3 &direction, float length, const Vector4 &color) {
    const Vector3 dir = direction.Length() > 1e-6f ? direction.Normalize() : Vector3(0.0f, -1.0f, 0.0f);
    const Vector3 end = origin + dir * length;
    out.push_back({ origin, color });
    out.push_back({ end, color });
}

void SceneEditorView::ApplyWorldMatrixToTransform(Transform *transform, const Matrix4x4 &worldMatrix) {
    if (!transform) return;

    // 親がいる場合はローカル行列へ変換してから適用する
    Matrix4x4 local = worldMatrix;
    if (auto *parentObject = transform->GetParentObject()) {
        if (auto *parentTransform = parentObject->GetComponent<Transform>()) {
            local = worldMatrix * parentTransform->GetWorldMatrix().Inverse();
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
}

void SceneEditorView::ShowGizmo(const std::unordered_set<EmptyObject *> &selectedObjects, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize) {
    // 選択集合が変わった場合は編集状態をリセットする
    if (selectedObjects != gizmoTargetObjects_) {
        gizmoTargetObjects_ = selectedObjects;
        isGizmoEditing_ = false;
    }

    std::vector<std::pair<EmptyObject *, Transform *>> targets;
    for (auto *obj : gizmoTargetObjects_) {
        if (!obj) continue;
        if (auto *transform = obj->GetComponent<Transform>()) {
            targets.emplace_back(obj, transform);
        }
    }
    if (targets.empty() || imageSize.x <= 0.0f || imageSize.y <= 0.0f) return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

    Matrix4x4 view = view_;
    Matrix4x4 projection = projection_;
    const bool isSingle = (targets.size() == 1);

    if (!ImGuizmo::IsUsing()) {
        // 未操作中はギズモの基準行列を毎フレーム最新の状態から作り直す。
        // 単一選択時は対象自身のワールド行列（Local/Worldモードの切り替えが意味を持つ）、
        // 複数選択時は選択群の平均位置を中心としたワールド軸固定の仮想行列にする
        // （バラバラな向きのオブジェクト群に共通のローカル基準は定義できないため）。
        if (isSingle) {
            groupGizmoMatrix_ = targets[0].second->GetWorldMatrix();
        } else {
            Vector3 pivot{ 0.0f, 0.0f, 0.0f };
            for (auto &[obj, transform] : targets) {
                const Matrix4x4 &world = transform->GetWorldMatrix();
                pivot = pivot + Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
            }
            pivot = pivot * (1.0f / static_cast<float>(targets.size()));
            groupGizmoMatrix_ = Matrix4x4::Identity();
            groupGizmoMatrix_.m[3][0] = pivot.x;
            groupGizmoMatrix_.m[3][1] = pivot.y;
            groupGizmoMatrix_.m[3][2] = pivot.z;
        }
    }

    // このフレームの増分（デルタ）を算出するため、Manipulate直前の状態を控えておく
    const Matrix4x4 beforeManipulate = groupGizmoMatrix_;

    ImGuizmo::Manipulate(
        &view.m[0][0], &projection.m[0][0],
        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
        isSingle ? static_cast<ImGuizmo::MODE>(gizmoMode_) : ImGuizmo::WORLD,
        &groupGizmoMatrix_.m[0][0]);

    if (ImGuizmo::IsUsing()) {
        // 操作開始時に変更前の状態を全対象分保存する（Undo用）
        if (!isGizmoEditing_) {
            isGizmoEditing_ = true;
            gizmoBeforeStates_.clear();
            for (auto &[obj, transform] : targets) {
                gizmoBeforeStates_.push_back({ obj, obj->SaveComponentToJson(transform) });
            }
        }

        // このフレームでギズモに加えられた増分（ワールド空間）を全対象へ均等に適用する
        // （row-vector規約: A' = A * D なら D = A^-1 * A'）
        const Matrix4x4 delta = beforeManipulate.Inverse() * groupGizmoMatrix_;
        for (auto &[obj, transform] : targets) {
            const Matrix4x4 newWorld = transform->GetWorldMatrix() * delta;
            ApplyWorldMatrixToTransform(transform, newWorld);
        }
    } else if (isGizmoEditing_) {
        // 操作終了時にUndo/Redo履歴へ積む（全対象をまとめて1操作にする）
        isGizmoEditing_ = false;
        if (commands) {
            auto composite = std::make_unique<CompositeCommand>(
                (gizmoBeforeStates_.size() > 1) ? ("Transform " + std::to_string(gizmoBeforeStates_.size()) + " Objects") : "Transform Object");
            bool anyChanged = false;
            for (auto &state : gizmoBeforeStates_) {
                if (!state.object) continue;
                auto *transform = state.object->GetComponent<Transform>();
                if (!transform) continue;
                JSON after = state.object->SaveComponentToJson(transform);
                if (after != state.before) {
                    anyChanged = true;
                    composite->AddCommand(std::make_unique<ComponentEditCommand>(state.object, transform, state.before, after));
                }
            }
            if (anyChanged) commands->PushExecuted(std::move(composite));
        }
        gizmoBeforeStates_.clear();
    }
}

} // namespace KashipanEngine

#endif // USE_IMGUI
