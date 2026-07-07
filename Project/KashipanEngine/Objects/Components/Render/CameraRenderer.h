#pragma once
#include <cstring>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Camera2D.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Transform.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Graphics/PipelineManager.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief 描画時に使用するカメラへ付与するコンポーネント
/// @details 同一オブジェクトの Transform と Camera3D（または Camera2D）から
///          ビュー射影行列を計算して定数バッファへアップロードする。
///          使用するパイプラインと、そのパイプラインに含まれるシェーダーの
///          どの定数バッファへバインドするかを指定できる。
class CameraRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(CameraRenderer, 1, SetUpdatePriority(950);)
    COMPONENT_CATEGORY("Render")
    ~CameraRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<CameraRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->bindVariableNames_ = bindVariableNames_;
        return ptr;
    }

    /// @brief 使用するパイプラインを設定（空文字の場合は全パイプラインに適用）
    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    //==================================================
    // 描画先指定
    //==================================================

    /// @brief 適用先の描画先オブジェクトを設定（未指定の場合は全描画先に適用）
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief 適用先の描画先オブジェクトをUUIDから設定
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    /// @brief 適用先の描画先オブジェクトのUUIDを取得
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 適用先の描画先オブジェクトを取得（未指定・存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    /// @brief バインド先の定数バッファ変数名を設定（例: "Vertex:gCamera3D"）
    void SetBindVariableNames(const std::vector<std::string> &names) { bindVariableNames_ = names; }
    const std::vector<std::string> &GetBindVariableNames() const noexcept { return bindVariableNames_; }

    /// @brief カメラ情報の定数バッファを取得
    ConstantBufferResource *GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

    /// @brief 最後にアップロードしたビュー射影行列を取得（シャドウマップ定数等に使用）
    const Matrix4x4 &GetViewProjectionMatrix() const noexcept { return lastViewProjection_; }
    /// @brief ニアクリップ距離を取得
    float GetNearClip() const noexcept { return lastNearClip_; }
    /// @brief ファークリップ距離を取得
    float GetFarClip() const noexcept { return lastFarClip_; }

protected:
    void Initialize() override {
        if (!constantBuffer_) {
            constantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(Camera3DConstant));
        }
        // バインド先が未指定の場合はカメラの種類から既定値を設定
        if (bindVariableNames_.empty()) {
            auto *objectContext = GetOwnerObjectContext();
            if (objectContext && objectContext->GetComponent<Camera2D>()) {
                bindVariableNames_ = { "Vertex:gCamera2D", "Pixel:gCamera2D" };
            } else {
                bindVariableNames_ = { "Vertex:gCamera3D", "Pixel:gCamera3D" };
            }
        }
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterCameraRenderer(this);
        }
        UploadCameraConstant();
    }

    void Update() override {
        UploadCameraConstant();
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterCameraRenderer(this);
        }
        constantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 適用先の描画先オブジェクトをシーン上から選択（D&D対応、未指定は全描画先）
        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);
        // パイプラインは読み込み済みのものから選択（未指定は全パイプライン）
        ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames(), true);
        for (const auto &name : bindVariableNames_) {
            ImGui::BulletText("%s", name.c_str());
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["pipelineName"] = pipelineName_;
        json["bindVariableNames"] = bindVariableNames_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        pipelineName_ = json.value("pipelineName", std::string{});
        bindVariableNames_.clear();
        if (json.contains("bindVariableNames")) {
            for (const auto &name : json["bindVariableNames"]) {
                bindVariableNames_.push_back(name.get<std::string>());
            }
        }
        return true;
    }

private:
    /// @brief gCamera3D 定数バッファと同レイアウトの構造体
    struct Camera3DConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
        Vector4 eyePosition;
        float fov = 0.0f;
        float padding[3]{};
    };
    /// @brief gCamera2D 定数バッファと同レイアウトの構造体
    struct Camera2DConstant {
        Matrix4x4 view;
        Matrix4x4 projection;
        Matrix4x4 viewProjection;
    };

    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    void UploadCameraConstant() {
        if (!constantBuffer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        auto *transform = objectContext->GetComponent<Transform>();
        const Matrix4x4 world = transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();
        const Matrix4x4 view = world.Inverse();

        void *mapped = constantBuffer_->Map();
        if (!mapped) return;

        if (auto *camera2d = objectContext->GetComponent<Camera2D>()) {
            Camera2DConstant constant{};
            constant.view = view;
            constant.projection.MakeOrthographicMatrix(
                0.0f, 0.0f, camera2d->GetWidth(), camera2d->GetHeight(),
                camera2d->GetNearClip(), camera2d->GetFarClip());
            constant.viewProjection = constant.view * constant.projection;
            lastViewProjection_ = constant.viewProjection;
            lastNearClip_ = camera2d->GetNearClip();
            lastFarClip_ = camera2d->GetFarClip();
            std::memcpy(mapped, &constant, sizeof(constant));
            return;
        }

        Camera3DConstant constant{};
        constant.view = view;
        float fovY = 0.45f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        float aspect = 16.0f / 9.0f;
        bool orthographic = false;
        float orthoSize = 10.0f;
        if (auto *camera3d = objectContext->GetComponent<Camera3D>()) {
            fovY = camera3d->GetFovY();
            nearClip = camera3d->GetNearClip();
            farClip = camera3d->GetFarClip();
            aspect = camera3d->GetAspectRatio();
            orthographic = camera3d->IsOrthographic();
            orthoSize = camera3d->GetOrthoSize();
        }
        if (orthographic) {
            constant.projection.MakeOrthographicMatrix(
                -orthoSize * aspect, orthoSize, orthoSize * aspect, -orthoSize, nearClip, farClip);
        } else {
            constant.projection.MakePerspectiveFovMatrix(fovY, aspect, nearClip, farClip);
        }
        constant.viewProjection = constant.view * constant.projection;
        constant.eyePosition = Vector4(world.m[3][0], world.m[3][1], world.m[3][2], 1.0f);
        constant.fov = fovY;
        lastViewProjection_ = constant.viewProjection;
        lastNearClip_ = nearClip;
        lastFarClip_ = farClip;
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    std::unique_ptr<ConstantBufferResource> constantBuffer_;
    UUID128 targetObjectID_{};
    std::string pipelineName_;
    std::vector<std::string> bindVariableNames_;

    // シャドウマップ定数等で参照するためのキャッシュ
    Matrix4x4 lastViewProjection_ = Matrix4x4::Identity();
    float lastNearClip_ = 0.1f;
    float lastFarClip_ = 1000.0f;
};

REGISTER_COMPONENT_OBJECT(CameraRenderer)

} // namespace KashipanEngine
