#pragma once
#ifdef USE_IMGUI
#include <imgui.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <numbers>

#include "Scene/SceneEditorContext.h"
#include "Scene/Editor/SceneEditorCommands.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Objects/Components/Transform.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class SceneEditor;

/// @brief シーンエディター用のシーンビュー
/// @details エディター専用の ScreenBuffer とデバッグカメラを持ち、
///          シーン上の全 MeshRenderer をこの ScreenBuffer に描画して ImGui ウィンドウへ表示する。
///          選択中オブジェクトには ImGuizmo によるギズモ操作が行える。
class SceneEditorView final {
public:
    SceneEditorView(Passkey<SceneEditor>, SceneEditorContext *context) : context_(context) {}
    ~SceneEditorView() {
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

    /// @brief シーンビューの描画（毎フレーム呼ぶ）
    /// @param selectedObject 選択中のシーンオブジェクト（ギズモ表示対象）
    /// @param commands ギズモ操作をUndo/Redo履歴へ積むためのコマンド管理
    void ShowImGui(EmptyObject *selectedObject, SceneEditorCommands *commands) {
        EnsureResources();
        UpdateCameraBuffer();
        RegisterEditorTarget();
        ShowSceneViewWindow(selectedObject, commands);
    }

private:
    /// @brief gCamera3D 定数バッファと同レイアウトの構造体
    struct CameraConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
        Vector4 eyePosition;
        float fov = 0.0f;
        float padding[3]{};
    };

    void EnsureResources() {
        if (!screenBuffer_ || !ScreenBuffer::IsExist(screenBuffer_)) {
            screenBuffer_ = ScreenBuffer::Create(1280, 720, "EditorSceneView");
        }
        if (!cameraBuffer_) {
            cameraBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CameraConstant));
        }
    }

    /// @brief デバッグカメラの行列を計算して定数バッファへアップロードする
    void UpdateCameraBuffer() {
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

    /// @brief SceneRenderer へエディター描画先として登録する
    void RegisterEditorTarget() {
        if (!context_) return;
        auto *sceneRenderer = context_->GetComponent<SceneRenderer>();
        if (sceneRenderer && screenBuffer_) {
            sceneRenderer->SetEditorTarget(screenBuffer_, cameraBuffer_.get());
        }
    }

    void ShowSceneViewWindow(EmptyObject *selectedObject, SceneEditorCommands *commands) {
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

        //--------- ImGuizmo によるギズモ表示 ---------//
        ShowGizmo(selectedObject, commands, imagePos, drawSize);

        ImGui::End();
    }

    void HandleCameraInput() {
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

    void ShowGizmo(EmptyObject *selectedObject, SceneEditorCommands *commands, const ImVec2 &imagePos, const ImVec2 &imageSize) {
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

            float translate[3]{}, rotateDeg[3]{}, scale[3]{};
            ImGuizmo::DecomposeMatrixToComponents(&local.m[0][0], translate, rotateDeg, scale);
            constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
            transform->SetTranslate(Vector3(translate[0], translate[1], translate[2]));
            transform->SetRotate(Vector3(rotateDeg[0] * kDegToRad, rotateDeg[1] * kDegToRad, rotateDeg[2] * kDegToRad));
            transform->SetScale(Vector3(scale[0], scale[1], scale[2]));
        } else if (isGizmoEditing_) {
            // 操作終了時にUndo/Redo履歴へ積む
            isGizmoEditing_ = false;
            JSON after = selectedObject->SaveComponentToJson(transform);
            if (commands && after != gizmoBefore_) {
                commands->PushExecuted(std::make_unique<ComponentEditCommand>(selectedObject, transform, gizmoBefore_, after));
            }
        }
    }

    SceneEditorContext *context_ = nullptr;

    ScreenBuffer *screenBuffer_ = nullptr;
    std::unique_ptr<ConstantBufferResource> cameraBuffer_;

    // デバッグカメラ（注視点周りのオービットカメラ）
    Vector3 target_{ 0.0f, 0.0f, 0.0f };
    float distance_ = 10.0f;
    float yaw_ = 0.0f;
    float pitch_ = 0.3f;
    float fovY_ = 0.45f;
    float nearClip_ = 0.1f;
    float farClip_ = 1000.0f;

    Matrix4x4 view_ = Matrix4x4::Identity();
    Matrix4x4 projection_ = Matrix4x4::Identity();

    // ギズモ状態
    int gizmoOperation_ = ImGuizmo::TRANSLATE;
    int gizmoMode_ = ImGuizmo::LOCAL;
    EmptyObject *gizmoTargetObject_ = nullptr;
    bool isGizmoEditing_ = false;
    JSON gizmoBefore_;
};

} // namespace KashipanEngine

#endif // USE_IMGUI
