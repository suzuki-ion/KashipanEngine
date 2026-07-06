#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Transform.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Graphics/PipelineManager.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief 描画時に使用するライトへ付与するコンポーネント
/// @details 同一オブジェクトの Transform と Light からライト情報を計算して
///          定数バッファへアップロードする。
///          使用するパイプラインと、そのパイプラインに含まれるシェーダーの
///          どの定数バッファへバインドするかを指定できる。
class LightRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(LightRenderer, 1, SetUpdatePriority(950);)
    ~LightRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<LightRenderer>();
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

    /// @brief バインド先の定数バッファ変数名を設定（例: "Pixel:gDirectionalLight"）
    void SetBindVariableNames(const std::vector<std::string> &names) { bindVariableNames_ = names; }
    const std::vector<std::string> &GetBindVariableNames() const noexcept { return bindVariableNames_; }

    /// @brief ライト情報の定数バッファを取得
    ConstantBufferResource *GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

    /// @brief 同一オブジェクトの Light コンポーネントを取得（存在しない場合は nullptr）
    Light *GetLight() const {
        auto *objectContext = GetOwnerObjectContext();
        return objectContext ? objectContext->GetComponent<Light>() : nullptr;
    }

    /// @brief ライトの種類を取得（Light コンポーネントが無い場合は Directional）
    Light::Type GetLightType() const {
        auto *light = GetLight();
        return light ? light->GetType() : Light::Type::Directional;
    }

    /// @brief ライトのワールド座標を取得（Transform が無い場合は原点）
    Vector3 GetWorldPosition() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return Vector3(0.0f, 0.0f, 0.0f);
        const Matrix4x4 &world = transform->GetWorldMatrix();
        return Vector3(world.m[3][0], world.m[3][1], world.m[3][2]);
    }

    /// @brief ライトのワールド方向（Transform の +Z）を取得
    Vector3 GetWorldDirection() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        Vector3 direction(0.0f, -1.0f, 0.0f);
        if (transform) {
            const Matrix4x4 &world = transform->GetWorldMatrix();
            Vector3 forward(world.m[2][0], world.m[2][1], world.m[2][2]);
            const float length = forward.Length();
            if (length > 0.0f) direction = forward * (1.0f / length);
        }
        return direction;
    }

protected:
    void Initialize() override {
        if (!constantBuffer_) {
            constantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DirectionalLightConstant));
        }
        if (bindVariableNames_.empty()) {
            bindVariableNames_ = { "Pixel:gDirectionalLight" };
        }
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterLightRenderer(this);
        }
        UploadLightConstant();
    }

    void Update() override {
        UploadLightConstant();
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterLightRenderer(this);
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
    /// @brief gDirectionalLight 定数バッファと同レイアウトの構造体
    struct DirectionalLightConstant {
        std::uint32_t enabled = 0;
        float padding[3]{};
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 direction{ 0.0f, -1.0f, 0.0f };
        float intensity = 1.0f;
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

    void UploadLightConstant() {
        if (!constantBuffer_) return;
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return;

        DirectionalLightConstant constant{};
        constant.enabled = IsActive() ? 1u : 0u;

        if (auto *light = objectContext->GetComponent<Light>()) {
            constant.color = light->GetColor();
            constant.intensity = light->GetIntensity();
            constant.enabled = (light->IsActive() && IsActive()) ? 1u : 0u;
        }

        // Transform の前方ベクトル（+Z）をライト方向とする
        if (auto *transform = objectContext->GetComponent<Transform>()) {
            const Matrix4x4 &world = transform->GetWorldMatrix();
            Vector3 forward(world.m[2][0], world.m[2][1], world.m[2][2]);
            const float length = forward.Length();
            if (length > 0.0f) {
                constant.direction = forward * (1.0f / length);
            }
        }

        void *mapped = constantBuffer_->Map();
        if (!mapped) return;
        std::memcpy(mapped, &constant, sizeof(constant));
    }

    std::unique_ptr<ConstantBufferResource> constantBuffer_;
    UUID128 targetObjectID_{};
    std::string pipelineName_;
    std::vector<std::string> bindVariableNames_;
};

REGISTER_COMPONENT_OBJECT(LightRenderer)

} // namespace KashipanEngine
