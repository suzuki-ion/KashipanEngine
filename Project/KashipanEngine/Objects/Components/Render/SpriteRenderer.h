#pragma once
#include <unordered_set>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/AssetDragDropPayload.h"
#endif

namespace KashipanEngine {

/// @brief 2Dスプライト描画用コンポーネント
/// @details Object2D系のパイプラインを使用する想定のコンポーネント。MeshRendererと同様に
///          描画するメッシュ（通常は平面）はMeshFilterコンポーネントから取得する。
///          描画先の指定方法・パイプライン/マテリアルの指定方法はMeshRendererと同じ。
class SpriteRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(SpriteRenderer, 0xFF, SetUpdatePriority(900);)
    COMPONENT_CATEGORY("Render")
    ~SpriteRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SpriteRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialName_ = materialName_;
        ptr->materialHandle_ = materialHandle_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        return ptr;
    }

    //==================================================
    // 描画先指定
    //==================================================

    /// @brief 描画先オブジェクトを設定（描画先コンポーネントが付与されたオブジェクト）
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    /// @brief 描画先オブジェクトをUUIDから設定
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    /// @brief 描画先オブジェクトのUUIDを取得
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    /// @brief 描画先オブジェクトを取得（存在しない場合は nullptr）
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }

    /// @brief 指定の描画先がこのコンポーネントの描画対象に含まれるか（除外設定されていないか）
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // パイプライン・マテリアル指定
    //==================================================

    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    void SetMaterialName(const std::string &materialName) {
        materialName_ = materialName;
        materialHandle_ = MaterialManager::kInvalidHandle;
    }
    void SetMaterialHandle(MaterialManager::MaterialHandle materialHandle) { materialHandle_ = materialHandle; }
    const std::string &GetMaterialName() const noexcept { return materialName_; }
    /// @brief マテリアルハンドルを取得（未解決の場合はマテリアル名から解決を試みる）
    MaterialManager::MaterialHandle GetMaterialHandle() const noexcept {
        if (materialHandle_ == MaterialManager::kInvalidHandle && !materialName_.empty()) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName(materialName_);
        }
        return materialHandle_;
    }

    //==================================================
    // 描画情報取得
    //==================================================

    /// @brief 描画に使用するメッシュハンドルを取得（MeshFilter コンポーネントから）
    ModelManager::ModelHandle GetMeshHandle() const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return ModelManager::kInvalidHandle;
        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        if (!meshFilter) return ModelManager::kInvalidHandle;
        return meshFilter->GetMeshHandle();
    }

    /// @brief ワールド行列を取得（Transform コンポーネントから）
    Matrix4x4 GetWorldMatrix() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        return transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();
    }

protected:
    void Initialize() override {
        auto *sceneRenderer = GetOrAddSceneRenderer();
        if (sceneRenderer) {
            sceneRenderer->RegisterSpriteRenderer(this);
        }
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterSpriteRenderer(this);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        // 描画先はシーン上のオブジェクトから選択（ヒエラルキーからのD&Dも受け付ける）
        TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);
        // 対象オブジェクトが持つ描画先ごとに描画する/しないを選択する
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
        // パイプラインとマテリアルは読み込み済みのものから選択する
        ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames());
        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) {
            materialNames.push_back(entry.material.name);
        }
        if (ImGuiCustom::SelectString("Material", materialName_, materialNames)) {
            materialHandle_ = MaterialManager::kInvalidHandle;
        }
        // Assetsウィンドウからのマテリアルファイルドラッグ&ドロップも受け付ける
        if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
            for (const auto &entry : materialEntries) {
                if (entry.assetPath == droppedPath) {
                    materialName_ = entry.material.name;
                    materialHandle_ = MaterialManager::kInvalidHandle;
                    break;
                }
            }
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["pipelineName"] = pipelineName_;
        json["materialName"] = materialName_;
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
        pipelineName_ = json.value("pipelineName", std::string{ "Object2D.DoubleSidedCulling.BlendNormal" });
        materialName_ = json.value("materialName", std::string{ "Default" });
        materialHandle_ = MaterialManager::kInvalidHandle;
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
    std::string pipelineName_ = "Object2D.DoubleSidedCulling.BlendNormal";
    std::string materialName_ = "Default";
    mutable MaterialManager::MaterialHandle materialHandle_ = MaterialManager::kInvalidHandle;
    /// @brief 除外する描画先の名前（GetRenderTargetName()）の集合
    std::unordered_set<std::string> excludedRenderTargetNames_;
};

REGISTER_COMPONENT_OBJECT(SpriteRenderer)

} // namespace KashipanEngine
