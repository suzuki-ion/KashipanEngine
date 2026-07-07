#include "SceneEditorView.h"
#ifdef USE_IMGUI
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Transform.h"
#include "Math/Quaternion.h"
#include "Utilities/MathUtils.h"

namespace KashipanEngine {

SceneEditorView::SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context)
    : context_(context), gizmoOperation_(ImGuizmo::TRANSLATE), gizmoMode_(ImGuizmo::LOCAL) {}

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

    //--------- シーンビュー画像 ---------//
    if (!screenBuffer_ || screenBuffer_->GetSrvHandle().ptr == 0) {
        ImGui::TextUnformatted("Scene view is not ready.");
        ImGui::End();
        return;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float bufferWidth = static_cast<float>(screenBuffer_->GetWidth());
    const float bufferHeight = static_cast<float>(screenBuffer_->GetHeight());
    ImVec2 drawSize = avail;
    if (bufferWidth > 0.0f && bufferHeight > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
        const float scale = std::min(avail.x / bufferWidth, avail.y / bufferHeight);
        drawSize = ImVec2(bufferWidth * scale, bufferHeight * scale);
    }
    const ImVec2 imagePos = ImGui::GetCursorScreenPos();
    ImGui::Image(static_cast<ImTextureID>(screenBuffer_->GetSrvHandle().ptr), drawSize);

    //--------- カメラ操作（画像上でのマウス操作） ---------//
    HandleCameraInput();

    //--------- XZ平面のグリッド線 ---------//
    DrawGrid(imagePos, drawSize);

    //--------- ライトのデバッグ表示 ---------//
    DrawLightMarkers(imagePos, drawSize);

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

bool SceneEditorView::ProjectToImage(const Vector3 &worldPosition, const ImVec2 &imagePos, const ImVec2 &imageSize, ImVec2 &outScreenPos) const {
    const Matrix4x4 viewProjection = view_ * projection_;
    const float x = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
    const float y = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
    const float w = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];
    if (w <= 1e-4f) return false;
    const float ndcX = x / w;
    const float ndcY = y / w;
    if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f) return false;
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
