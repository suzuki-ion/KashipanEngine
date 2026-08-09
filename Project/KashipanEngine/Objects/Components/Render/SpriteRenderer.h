#pragma once
#include <unordered_set>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Math/Vector2.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief 2Dスプライト描画用コンポーネント
/// @details Object2D系のパイプラインを使用する想定のコンポーネント。MeshRendererと同様に
///          描画するメッシュ（通常は平面）はMeshFilterコンポーネントから取得する。
///          描画先の指定方法・パイプライン/マテリアルの指定方法はMeshRendererと同じ。
///          メッシュはXY平面上の-0.5～0.5の単位クアッド（Rect2D等）を想定しており、
///          アンカー・ピボットはその単位クアッド内の正規化座標(0,0)=左下 ～ (1,1)=右上で指定する。
class SpriteRenderer final : public IObjectComponent {
public:
    // pipelineName_/materialName_の直接書き込み時は、セッターと同様に描画リストの再構築
    // （materialName_はハンドルの再解決も）を促す
    OBJECT_COMPONENT_CONSTRUCTOR(SpriteRenderer, 0xFF,
        SetUpdatePriority(900);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(pipelineName_, [this] { MarkDrawListDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(materialName_, [this] {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkDrawListDirty();
        });
        ADD_MEMBER_VARIABLE(anchor_);
        ADD_MEMBER_VARIABLE(pivot_);
    )
    COMPONENT_CATEGORY("Render")
    ~SpriteRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SpriteRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialName_ = materialName_;
        ptr->materialHandle_ = materialHandle_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->anchor_ = anchor_;
        ptr->pivot_ = pivot_;
        return ptr;
    }

    //==================================================
    // 描画先指定
    //==================================================

    /// @brief 描画先オブジェクトを設定（描画先コンポーネントが付与されたオブジェクト）
    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
        MarkDrawListDirty();
    }
    /// @brief 描画先オブジェクトをUUIDから設定
    void SetTargetObject(const UUID128 &targetObjectID) {
        targetObjectID_ = targetObjectID;
        MarkDrawListDirty();
    }
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

    void SetPipelineName(const std::string &pipelineName) {
        pipelineName_ = pipelineName;
        MarkDrawListDirty();
    }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    void SetMaterialName(const std::string &materialName) {
        materialName_ = materialName;
        materialHandle_ = MaterialManager::kInvalidHandle;
        MarkDrawListDirty();
    }
    void SetMaterialHandle(MaterialManager::MaterialHandle materialHandle) {
        materialHandle_ = materialHandle;
        MarkDrawListDirty();
    }
    const std::string &GetMaterialName() const noexcept { return materialName_; }
    /// @brief マテリアルハンドルを取得（未解決の場合はマテリアル名から解決を試みる）
    MaterialManager::MaterialHandle GetMaterialHandle() const noexcept {
        if (materialHandle_ == MaterialManager::kInvalidHandle && !materialName_.empty()) {
            materialHandle_ = MaterialManager::GetMaterialHandleFromName(materialName_);
        }
        return materialHandle_;
    }

    //==================================================
    // アンカー・ピボット
    //==================================================

    /// @brief アンカーを設定する
    /// @details Transformの座標に一致させるメッシュ上の点（単位クアッド内の正規化座標。
    ///          (0,0)=左下 ～ (1,1)=右上、既定値は(0.5,0.5)=中心）
    void SetAnchor(const Vector2 &anchor) { anchor_ = anchor; }
    const Vector2 &GetAnchor() const noexcept { return anchor_; }

    /// @brief ピボットを設定する
    /// @details 回転・拡縮の中心にするメッシュ上の点（単位クアッド内の正規化座標。
    ///          アンカーと異なる点を指定した場合、回転・拡縮の中心はアンカー（＝Transformの座標）とは
    ///          別の位置になる。既定値は(0.5,0.5)=中心）
    void SetPivot(const Vector2 &pivot) { pivot_ = pivot; }
    const Vector2 &GetPivot() const noexcept { return pivot_; }

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

    /// @brief ワールド行列を取得（Transform コンポーネントに、アンカー・ピボットによる補正を加えたもの）
    /// @details メッシュ（単位クアッド、-0.5～0.5）のピボット点が回転・拡縮の中心兼ワールド行列の
    ///          原点となるように平行移動し、続けてアンカー点がTransformのワールド座標に一致するように
    ///          再度平行移動で補正する（アンカーとピボットが同じ場合は何も変化しない）。
    Matrix4x4 GetWorldMatrix() const {
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        if (!transform) return Matrix4x4::Identity();

        Matrix4x4 world = transform->GetWorldMatrix();

        // ピボット: 単位クアッド上のこの点が新しい原点(回転・拡縮の中心)になるよう平行移動成分へ加算する
        // (行ベクトル規約のため、平行移動量はワールド行列の回転・拡縮部分(行0,1)で変換してから加える)
        const float pivotOffsetX = 0.5f - pivot_.x;
        const float pivotOffsetY = 0.5f - pivot_.y;
        if (pivotOffsetX != 0.0f || pivotOffsetY != 0.0f) {
            world.m[3][0] += pivotOffsetX * world.m[0][0] + pivotOffsetY * world.m[1][0];
            world.m[3][1] += pivotOffsetX * world.m[0][1] + pivotOffsetY * world.m[1][1];
            world.m[3][2] += pivotOffsetX * world.m[0][2] + pivotOffsetY * world.m[1][2];
        }

        // アンカー: ピボットとは独立して、この点がTransformのワールド座標に一致するように補正する
        const float anchorDeltaX = anchor_.x - pivot_.x;
        const float anchorDeltaY = anchor_.y - pivot_.y;
        if (anchorDeltaX != 0.0f || anchorDeltaY != 0.0f) {
            world.m[3][0] -= anchorDeltaX * world.m[0][0] + anchorDeltaY * world.m[1][0];
            world.m[3][1] -= anchorDeltaX * world.m[0][1] + anchorDeltaY * world.m[1][1];
            world.m[3][2] -= anchorDeltaX * world.m[0][2] + anchorDeltaY * world.m[1][2];
        }

        return world;
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
        if (TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_)) {
            MarkDrawListDirty();
        }
        // 対象オブジェクトが持つ描画先ごとに描画する/しないを選択する
        TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
        // パイプラインとマテリアルは読み込み済みのものから選択する（2D用途のもののみに絞り込む）
        if (ImGuiCustom::SelectString(TranslationLabel("component.spriterenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("2D"))) {
            MarkDrawListDirty();
        }
        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) {
            materialNames.push_back(entry.material.name);
        }
        if (ImGuiCustom::SelectString(TranslationLabel("component.spriterenderer.material"), materialName_, materialNames)) {
            materialHandle_ = MaterialManager::kInvalidHandle;
            MarkDrawListDirty();
        }
        // Assetsウィンドウからのマテリアルファイルドラッグ&ドロップも受け付ける
        if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
            for (const auto &entry : materialEntries) {
                if (entry.assetPath == droppedPath) {
                    materialName_ = entry.material.name;
                    materialHandle_ = MaterialManager::kInvalidHandle;
                    MarkDrawListDirty();
                    break;
                }
            }
        }
        // アンカー・ピボット（単位クアッド内の正規化座標。(0,0)=左下 ～ (1,1)=右上）
        ImGui::DragFloat2(TranslationLabel("component.spriterenderer.anchor"), &anchor_.x, 0.01f);
        ImGui::DragFloat2(TranslationLabel("component.spriterenderer.pivot"), &pivot_.x, 0.01f);
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
        json["anchor"] = ToJSON(anchor_);
        json["pivot"] = ToJSON(pivot_);
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
        anchor_ = json.contains("anchor") ? FromJSON<Vector2>(json["anchor"]) : Vector2(0.5f, 0.5f);
        pivot_ = json.contains("pivot") ? FromJSON<Vector2>(json["pivot"]) : Vector2(0.5f, 0.5f);
        // Undo/Redo等、登録済みのコンポーネントに対してもLoadFromJsonが呼ばれ得るため念のため通知する
        MarkDrawListDirty();
        return true;
    }

private:
    /// @brief パイプライン名・マテリアル・描画先指定など、描画リストのソート結果に影響するプロパティを
    ///        変更した際に呼ぶ（SceneRendererが未登録の場合は何もしない。登録済みなら次回のBuildSortedDrawList
    ///        でキャッシュが再構築される）
    void MarkDrawListDirty() const {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) sceneRenderer->MarkDrawListDirty();
    }

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

    /// @brief アンカー（Transformの座標に一致させるメッシュ上の点。単位クアッド内の正規化座標）
    Vector2 anchor_{ 0.5f, 0.5f };
    /// @brief ピボット（回転・拡縮の中心にするメッシュ上の点。単位クアッド内の正規化座標）
    Vector2 pivot_{ 0.5f, 0.5f };
};

REGISTER_COMPONENT_OBJECT(SpriteRenderer)

} // namespace KashipanEngine
