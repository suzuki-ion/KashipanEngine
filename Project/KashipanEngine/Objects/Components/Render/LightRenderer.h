#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>

#include "Objects/ObjectComponentHeader.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Transform.h"
#include "Math/Vector3.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#endif

namespace KashipanEngine {

/// @brief 描画時に使用するライトへ付与するコンポーネント
/// @details 同一オブジェクトの Transform と Light からライト情報を取得し、
///          Renderer がライトの種類ごとの構造化バッファ
///          （gDirectionalLights / gPointLights / gSpotLights）へまとめて送る。
///          使用するパイプラインと適用先の描画先を指定できる。
class LightRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(LightRenderer, 0xFF, SetUpdatePriority(950);)
    COMPONENT_CATEGORY("Render")
    ~LightRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<LightRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
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

    /// @brief 指定の描画先がこのコンポーネントの適用対象に含まれるか（除外設定されていないか）
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // ライト情報取得（Renderer が構造化バッファへ詰めるために使用）
    //==================================================

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
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterLightRenderer(this);
        }
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterLightRenderer(this);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 適用先の描画先オブジェクトをシーン上から選択（D&D対応、未指定は全描画先）
        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);
        // 対象オブジェクトが持つ描画先ごとに適用する/しないを選択する
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
        // パイプラインは読み込み済みのものから選択（未指定は全パイプライン）
        ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames(), true);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["pipelineName"] = pipelineName_;
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        pipelineName_ = json.value("pipelineName", std::string{});
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        return true;
    }

private:
    SceneRenderer *GetOrAddSceneRenderer() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext) return nullptr;
        auto *sceneRenderer = sceneContext->GetComponent<SceneRenderer>();
        if (!sceneRenderer) {
            sceneRenderer = sceneContext->AddComponent<SceneRenderer>();
        }
        return sceneRenderer;
    }

    UUID128 targetObjectID_{};
    std::string pipelineName_;
    /// @brief 除外する描画先の名前（GetRenderTargetName()）の集合
    std::unordered_set<std::string> excludedRenderTargetNames_;
};

REGISTER_COMPONENT_OBJECT(LightRenderer)

} // namespace KashipanEngine
