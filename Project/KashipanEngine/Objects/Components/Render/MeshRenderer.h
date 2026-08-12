#pragma once
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

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
#include "Objects/Components/Render/PipelineVariantBuilder.h"
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/AssetDragDropPayload.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

/// @brief メッシュ描画用コンポーネント
/// @details 描画先は「描画先コンポーネント（NormalWindowObject / OverlayWindowObject /
///          ScreenBufferObject / ShadowMapObject）が付与されたオブジェクト」を指定する。
///          指定オブジェクトに付与された全ての描画先に対して描画が行われる。
class MeshRenderer final : public IObjectComponent {
public:
    /// @brief インスタンスカラー（オブジェクト単位の色）をマテリアルの色へ適用する方法
    enum class ColorBlendMode : int {
        Override = 0, ///< マテリアルの色を無視してインスタンスカラーで置き換える
        Multiply,      ///< マテリアルの色に乗算する（既定。白(1,1,1,1)なら見た目に影響しない）
        Add,           ///< マテリアルの色に加算する
        Subtract,      ///< マテリアルの色から減算する
    };

    // pipelineName_の直接書き込み時は、セッターと同様に描画リストの再構築を促す
    // （マテリアルはスロット制のvector管理のため、要素アドレスが変わり得ずメンバ変数登録できない）
    OBJECT_COMPONENT_CONSTRUCTOR(MeshRenderer, 0xFF,
        SetUpdatePriority(900);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(pipelineName_, [this] { MarkDrawListDirty(); });
        ADD_MEMBER_VARIABLE(castShadows_);
        ADD_MEMBER_VARIABLE(instanceColor_);
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(renderPriority_, [this] { MarkDrawListDirty(); });
        ADD_MEMBER_VARIABLE_WITH_CALLBACK(allowInstancing_, [this] { MarkDrawListDirty(); });
    )
    COMPONENT_CATEGORY("Render")
    ~MeshRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MeshRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialNames_ = materialNames_;
        ptr->materialHandles_ = materialHandles_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->castShadows_ = castShadows_;
        ptr->instanceColor_ = instanceColor_;
        ptr->instanceColorBlendMode_ = instanceColorBlendMode_;
        ptr->renderPriority_ = renderPriority_;
        ptr->allowInstancing_ = allowInstancing_;
        return ptr;
    }

    //==================================================
    // インスタンスカラー（オブジェクト単位の色）
    //==================================================

    /// @brief インスタンスカラーを設定する（マテリアルの色へ適用される色。既定は白(1,1,1,1)＋Multiplyで無効化と同義）
    void SetInstanceColor(const Vector4 &color) noexcept { instanceColor_ = color; }
    const Vector4 &GetInstanceColor() const noexcept { return instanceColor_; }
    /// @brief インスタンスカラーをマテリアルの色へ適用する方法を設定する
    void SetInstanceColorBlendMode(ColorBlendMode mode) noexcept { instanceColorBlendMode_ = mode; }
    ColorBlendMode GetInstanceColorBlendMode() const noexcept { return instanceColorBlendMode_; }

    //==================================================
    // 描画順・インスタンシング制御
    //==================================================

    /// @brief 描画順を制御する優先度を設定する（既定0）。値が小さいほど先（奥）、大きいほど後（手前）に
    ///        描画される。パイプラインの描画優先度より後、メッシュ/マテリアルによる並びより前に評価される
    void SetRenderPriority(std::int32_t priority) noexcept { renderPriority_ = priority; MarkDrawListDirty(); }
    std::int32_t GetRenderPriority() const noexcept { return renderPriority_; }
    /// @brief このレンダラーを他のオブジェクトとのインスタンシング（1回のドローコールへのバッチ結合）
    ///        対象にするか設定する（既定true）。falseにすると、同じメッシュ/マテリアル/パイプラインを
    ///        共有する他のオブジェクトがあっても常に単独のドローコールで描画される
    void SetAllowInstancing(bool allow) noexcept { allowInstancing_ = allow; MarkDrawListDirty(); }
    bool GetAllowInstancing() const noexcept { return allowInstancing_; }

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

    /// @brief シャドウマッピングのシャドウキャスターとして扱うかを設定する
    void SetCastShadows(bool enabled) noexcept { castShadows_ = enabled; }
    bool GetCastShadows() const noexcept { return castShadows_; }

    void SetMaterialName(const std::string &materialName) { SetMaterialNameAt(0, materialName); }
    void SetMaterialHandle(MaterialManager::MaterialHandle materialHandle) {
        materialHandles_[0] = materialHandle;
        MarkDrawListDirty();
    }
    const std::string &GetMaterialName() const noexcept { return materialNames_.front(); }
    /// @brief マテリアルハンドルを取得（未解決の場合はマテリアル名から解決を試みる）
    MaterialManager::MaterialHandle GetMaterialHandle() const noexcept { return GetMaterialHandleAt(0); }

    //==================================================
    // マテリアルスロット（サブメッシュごとのマテリアル。Unityと同様スロットi＝サブメッシュi）
    //==================================================

    /// @brief マテリアルスロット数を取得（常に1以上）
    size_t GetMaterialSlotCount() const noexcept { return materialNames_.size(); }
    /// @brief マテリアルスロット数を変更する（追加分は最後のスロットと同じマテリアルで埋める）
    void SetMaterialSlotCount(size_t count) {
        if (count < 1) count = 1;
        if (count == materialNames_.size()) return;
        materialNames_.resize(count, materialNames_.back());
        materialHandles_.resize(count, MaterialManager::kInvalidHandle);
        MarkDrawListDirty();
    }
    /// @brief 指定スロットのマテリアル名を設定する（スロットが足りない場合は拡張される）
    void SetMaterialNameAt(size_t slot, const std::string &materialName) {
        if (slot >= materialNames_.size()) SetMaterialSlotCount(slot + 1);
        materialNames_[slot] = materialName;
        materialHandles_[slot] = MaterialManager::kInvalidHandle;
        MarkDrawListDirty();
    }
    /// @brief 指定スロットのマテリアル名を取得する（範囲外は最後のスロットを返す）
    const std::string &GetMaterialNameAt(size_t slot) const noexcept {
        return materialNames_[std::min(slot, materialNames_.size() - 1)];
    }
    /// @brief 指定スロットのマテリアルハンドルを取得（未解決の場合はマテリアル名から解決を試みる）
    /// @details スロット数を超えるサブメッシュは最後のスロットのマテリアルで描画される（Unityと同様）
    MaterialManager::MaterialHandle GetMaterialHandleAt(size_t slot) const noexcept {
        const size_t index = std::min(slot, materialNames_.size() - 1);
        if (materialHandles_[index] == MaterialManager::kInvalidHandle && !materialNames_[index].empty()) {
            materialHandles_[index] = MaterialManager::GetMaterialHandleFromName(materialNames_[index]);
        }
        return materialHandles_[index];
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
            sceneRenderer->RegisterMeshRenderer(this);
        }
    }

    void Finalize() override {
        auto *sceneContext = GetOwnerSceneContext();
        auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
        if (sceneRenderer) {
            sceneRenderer->UnregisterMeshRenderer(this);
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
        // パイプラインとマテリアルは読み込み済みのものから選択する（3D用途のもののみに絞り込む）
        if (ImGuiCustom::SelectString(TranslationLabel("component.meshrenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("3D"))) {
            MarkDrawListDirty();
        }
        // カスタムバリアントビルダー: 事前列挙されたBlend×Culling×Toon×シェーダーモジュールの組み合わせに
        // 無いパイプライン名が欲しい場合、ここでトークンを組み立てて PipelineManager::TryGetOrCreatePipeline
        // に動的生成させる（MeshRenderer/SkinnedMeshRenderer共通のUI。実装はPipelineVariantBuilder参照）
        if (PipelineVariantBuilder::Show(pipelineName_)) {
            MarkDrawListDirty();
        }
        ImGui::Checkbox(TranslationLabel("component.meshrenderer.cast_shadows"), &castShadows_);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.meshrenderer.desc_1"));
        }

        ImGui::ColorEdit4(TranslationLabel("component.meshrenderer.instance_color"), &instanceColor_.x);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.meshrenderer.desc_2"));
        }
        const char *kColorBlendModeLabels[] = { TranslationC("component.common.blendmode.override"), TranslationC("component.common.blendmode.multiply"), TranslationC("component.common.blendmode.add"), TranslationC("component.common.blendmode.subtract") };
        int blendModeIndex = static_cast<int>(instanceColorBlendMode_);
        if (ImGui::Combo(TranslationLabel("component.meshrenderer.instance_color_blend_mode"), &blendModeIndex, kColorBlendModeLabels, IM_ARRAYSIZE(kColorBlendModeLabels))) {
            instanceColorBlendMode_ = static_cast<ColorBlendMode>(blendModeIndex);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.meshrenderer.desc_3"));
        }

        if (ImGui::DragInt(TranslationLabel("component.common.render_priority"), &renderPriority_)) {
            MarkDrawListDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.common.desc_render_priority"));
        }
        if (ImGui::Checkbox(TranslationLabel("component.common.allow_instancing"), &allowInstancing_)) {
            MarkDrawListDirty();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.common.desc_allow_instancing"));
        }

        const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
        std::vector<std::string> materialNames;
        for (const auto &entry : materialEntries) {
            materialNames.push_back(entry.material.name);
        }

        // マテリアルスロット（メッシュのサブメッシュ数に合わせて表示する）
        const auto meshHandle = GetMeshHandle();
        if (meshHandle != ModelManager::kInvalidHandle) {
            const size_t subMeshCount = std::max<size_t>(1, ModelManager::GetModelData(meshHandle).GetSubMeshes().size());
            if (materialNames_.size() != subMeshCount) SetMaterialSlotCount(subMeshCount);
        }
        for (size_t i = 0; i < materialNames_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            char label[32];
            if (materialNames_.size() == 1) {
                std::snprintf(label, sizeof(label), "Material");
            } else {
                std::snprintf(label, sizeof(label), "Material %zu", i);
            }
            if (ImGuiCustom::SelectString(label, materialNames_[i], materialNames)) {
                materialHandles_[i] = MaterialManager::kInvalidHandle;
                MarkDrawListDirty();
            }
            // Assetsウィンドウからのマテリアルファイルドラッグ&ドロップも受け付ける
            if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
                for (const auto &entry : materialEntries) {
                    if (entry.assetPath == droppedPath) {
                        materialNames_[i] = entry.material.name;
                        materialHandles_[i] = MaterialManager::kInvalidHandle;
                        MarkDrawListDirty();
                        break;
                    }
                }
            }
            ImGui::PopID();
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["targetObjectID"] = ToJSON(targetObjectID_);
        json["pipelineName"] = pipelineName_;
        // 後方互換のため単一の"materialName"（スロット0）も併記する
        json["materialName"] = materialNames_.front();
        json["materialNames"] = materialNames_;
        for (const auto &name : excludedRenderTargetNames_) {
            json["excludedRenderTargetNames"].push_back(name);
        }
        json["castShadows"] = castShadows_;
        json["instanceColor"] = ToJSON(instanceColor_);
        json["instanceColorBlendMode"] = static_cast<int>(instanceColorBlendMode_);
        json["renderPriority"] = renderPriority_;
        json["allowInstancing"] = allowInstancing_;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("targetObjectID")) {
            targetObjectID_ = FromJSON<UUID128>(json["targetObjectID"]);
        } else {
            targetObjectID_ = UUID128();
        }
        pipelineName_ = json.value("pipelineName", std::string{ "Object3D.Solid.BlendNormal" });
        if (json.contains("materialNames") && json["materialNames"].is_array() && !json["materialNames"].empty()) {
            materialNames_ = json["materialNames"].get<std::vector<std::string>>();
        } else {
            // 旧形式（単一の"materialName"）との後方互換
            materialNames_ = { json.value("materialName", std::string{ "Default" }) };
        }
        materialHandles_.assign(materialNames_.size(), MaterialManager::kInvalidHandle);
        excludedRenderTargetNames_.clear();
        for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
            excludedRenderTargetNames_.insert(name);
        }
        castShadows_ = json.value("castShadows", true);
        instanceColor_ = json.contains("instanceColor") ? FromJSON<Vector4>(json["instanceColor"]) : Vector4(1.0f, 1.0f, 1.0f, 1.0f);
        instanceColorBlendMode_ = static_cast<ColorBlendMode>(json.value("instanceColorBlendMode", static_cast<int>(ColorBlendMode::Multiply)));
        renderPriority_ = json.value("renderPriority", 0);
        allowInstancing_ = json.value("allowInstancing", true);
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
    std::string pipelineName_ = "Object3D.Solid.BlendNormal";
    /// @brief マテリアルスロット（サブメッシュごとのマテリアル名。常に1要素以上）
    std::vector<std::string> materialNames_{ "Default" };
    /// @brief materialNames_と対応するハンドルのキャッシュ（未解決はkInvalidHandle）
    mutable std::vector<MaterialManager::MaterialHandle> materialHandles_{ MaterialManager::kInvalidHandle };
    /// @brief 除外する描画先の名前（GetRenderTargetName()）の集合
    std::unordered_set<std::string> excludedRenderTargetNames_;
    /// @brief シャドウマッピングのシャドウキャスターとして扱うか
    bool castShadows_ = true;
    /// @brief オブジェクト単位の色（マテリアルは共有したまま、この色をinstanceColorBlendMode_で適用する）
    Vector4 instanceColor_{ 1.0f, 1.0f, 1.0f, 1.0f };
    ColorBlendMode instanceColorBlendMode_ = ColorBlendMode::Multiply;
    /// @brief 描画順を制御する優先度（既定0。SceneRenderer::CompareSortableEntry参照）
    int renderPriority_ = 0;
    /// @brief 他のオブジェクトとのインスタンシング（バッチ結合）を許可するか（既定true）
    bool allowInstancing_ = true;
};

REGISTER_COMPONENT_OBJECT(MeshRenderer)

} // namespace KashipanEngine
