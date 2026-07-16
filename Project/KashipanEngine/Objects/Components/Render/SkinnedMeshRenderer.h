#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Objects/ObjectComponentHeader.h"
#include "Assets/MaterialManager.h"
#include "Assets/ModelManager.h"
#include "Assets/SkeletonManager.h"
#include "Assets/AnimationManager.h"
#include "Graphics/IRenderTarget.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Math/Matrix4x4.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Scene/Components/Render/SceneRenderer.h"
#include "Utilities/Passkeys.h"
#include "Utilities/UUID128.h"
#if defined(USE_IMGUI)
#include "Objects/Components/Render/TargetObjectSelector.h"
#include "Utilities/AssetDragDropPayload.h"
#endif

namespace KashipanEngine {

class Renderer;

/// @brief GPUスキニングの品質（UnityのSkinQualityに相当。頂点あたりの参照ボーン数の上限）
enum class SkinQuality {
    Auto = 0,
    Bone1,
    Bone2,
    Bone4,
};

/// @brief BlendShape（モーフターゲット）1つ分のウェイト設定
/// @details 一覧はメッシュ（ModelData::GetBlendShapes）から自動的に同期される
///          （UnityのSkinnedMeshRendererと同様、コンポーネント側で追加・削除はできない）。
///          ユーザーが編集できるのはウェイトのみ。
struct BlendShapeWeight final {
    std::string name;
    /// @brief ウェイト（0～100、Unityと同じ表現）
    float weight = 0.0f;
};

/// @brief スキンメッシュ（ボーンアニメーションで変形するメッシュ）描画用コンポーネント
/// @details 基本的な描画先・パイプライン・マテリアル指定はMeshRendererと同じだが、
///          MeshFilterのメッシュに紐づくスケルトン・アニメーションを使ってGPUスキニングを行い、
///          その結果（スキニング済み頂点バッファ）を描画に使用する。
///          GPUスキニング自体はSkinningパイプライン（Computeシェーダー）で行われ、
///          毎フレームRendererから駆動される。
class SkinnedMeshRenderer final : public IObjectComponent {
public:
    OBJECT_COMPONENT_CONSTRUCTOR(SkinnedMeshRenderer, 0xFF, SetUpdatePriority(900);)
    COMPONENT_CATEGORY("Render")
    ~SkinnedMeshRenderer() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<SkinnedMeshRenderer>();
        ptr->targetObjectID_ = targetObjectID_;
        ptr->rootBoneObjectID_ = rootBoneObjectID_;
        ptr->pipelineName_ = pipelineName_;
        ptr->materialNames_ = materialNames_;
        ptr->materialHandles_ = materialHandles_;
        ptr->excludedRenderTargetNames_ = excludedRenderTargetNames_;
        ptr->quality_ = quality_;
        ptr->blendShapes_ = blendShapes_;
        ptr->animationClipName_ = animationClipName_;
        ptr->playOnStart_ = playOnStart_;
        ptr->loop_ = loop_;
        ptr->playbackSpeed_ = playbackSpeed_;
        return ptr;
    }

    //==================================================
    // 描画先指定（MeshRendererと同じ）
    //==================================================

    void SetTargetObject(const EmptyObject *targetObject) {
        targetObjectID_ = targetObject ? targetObject->GetObjectID() : UUID128();
    }
    void SetTargetObject(const UUID128 &targetObjectID) { targetObjectID_ = targetObjectID; }
    const UUID128 &GetTargetObjectID() const noexcept { return targetObjectID_; }
    EmptyObject *GetTargetObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !targetObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(targetObjectID_);
    }
    bool IsRenderTargetIncluded(const IRenderTarget *target) const {
        if (!target) return false;
        return !excludedRenderTargetNames_.contains(target->GetRenderTargetName());
    }

    //==================================================
    // パイプライン・マテリアル指定（MeshRendererと同じ）
    //==================================================

    void SetPipelineName(const std::string &pipelineName) { pipelineName_ = pipelineName; }
    const std::string &GetPipelineName() const noexcept { return pipelineName_; }

    void SetMaterialName(const std::string &materialName) { SetMaterialNameAt(0, materialName); }
    void SetMaterialHandle(MaterialManager::MaterialHandle materialHandle) { materialHandles_[0] = materialHandle; }
    const std::string &GetMaterialName() const noexcept { return materialNames_.front(); }
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
    }
    /// @brief 指定スロットのマテリアル名を設定する（スロットが足りない場合は拡張される）
    void SetMaterialNameAt(size_t slot, const std::string &materialName) {
        if (slot >= materialNames_.size()) SetMaterialSlotCount(slot + 1);
        materialNames_[slot] = materialName;
        materialHandles_[slot] = MaterialManager::kInvalidHandle;
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
    // Skinned Mesh Renderer 固有の設定
    //==================================================

    void SetQuality(SkinQuality quality) { quality_ = quality; }
    SkinQuality GetQuality() const noexcept { return quality_; }

    /// @brief Root Boneオブジェクトを設定する（Unityと同様）
    /// @details 設定するとメッシュの描画位置がこのオブジェクトのTransformに沿って動く。
    ///          未設定（無効なUUID）の場合は所有オブジェクト自身のTransformを使う
    void SetRootBoneObject(const EmptyObject *rootBoneObject) {
        rootBoneObjectID_ = rootBoneObject ? rootBoneObject->GetObjectID() : UUID128();
    }
    void SetRootBoneObject(const UUID128 &rootBoneObjectID) { rootBoneObjectID_ = rootBoneObjectID; }
    const UUID128 &GetRootBoneObjectID() const noexcept { return rootBoneObjectID_; }
    EmptyObject *GetRootBoneObject() const {
        auto *sceneContext = GetOwnerSceneContext();
        if (!sceneContext || !rootBoneObjectID_.IsValid()) return nullptr;
        return sceneContext->GetSceneObject(rootBoneObjectID_);
    }

    const std::vector<BlendShapeWeight> &GetBlendShapes() const noexcept { return blendShapes_; }
    void SetBlendShapeWeight(const std::string &name, float weight);
    float GetBlendShapeWeight(const std::string &name) const;

    /// @brief 再生するアニメーションクリップ名を設定（MeshFilterのメッシュと同じアセットから読み込まれた
    ///        AnimationClipの中から選択する。空文字の場合はバインドポーズのまま静止する）
    void SetAnimationClipName(const std::string &clipName) { animationClipName_ = clipName; elapsedTime_ = 0.0f; }
    const std::string &GetAnimationClipName() const noexcept { return animationClipName_; }
    void SetPlayOnStart(bool playOnStart) { playOnStart_ = playOnStart; }
    bool GetPlayOnStart() const noexcept { return playOnStart_; }
    void SetLoop(bool loop) { loop_ = loop; }
    bool GetLoop() const noexcept { return loop_; }
    void SetPlaybackSpeed(float speed) { playbackSpeed_ = speed; }
    float GetPlaybackSpeed() const noexcept { return playbackSpeed_; }

    void Play() { isPlaying_ = true; }
    void Stop() { isPlaying_ = false; }
    bool IsPlaying() const noexcept { return isPlaying_; }

    /// @brief このコンポーネント専用のスケルトンインスタンスの姿勢をバインドポーズへ戻し、
    ///        アニメーション経過時間もリセットする（ゲームループ停止時にSceneRenderer経由で呼ばれる）
    void ResetAnimationToBindPose() {
        for (auto &joint : skeletonInstance_.joints) {
            if (auto *t = joint.transform.get()) t->ResetToBindPose();
            if (auto *t = joint.skeletonSpaceTransform.get()) t->ResetToBindPose();
        }
        elapsedTime_ = 0.0f;
    }

    /// @brief 選択中メッシュのアセットに紐づくアニメーションクリップ名の一覧を取得
    std::vector<std::string> GetAvailableAnimationClipNames() const;

    //==================================================
    // 描画情報取得
    //==================================================

    ModelManager::ModelHandle GetMeshHandle() const {
        auto *objectContext = GetOwnerObjectContext();
        if (!objectContext) return ModelManager::kInvalidHandle;
        auto *meshFilter = objectContext->GetComponent<MeshFilter>();
        if (!meshFilter) return ModelManager::kInvalidHandle;
        return meshFilter->GetMeshHandle();
    }

    Matrix4x4 GetWorldMatrix() const {
        // Root Boneが設定されている場合は、そのオブジェクトのTransformに沿って動く（Unityと同様）
        if (auto *rootBone = GetRootBoneObject()) {
            if (auto *rootBoneTransform = rootBone->GetComponent<Transform>()) {
                return rootBoneTransform->GetWorldMatrix();
            }
        }
        auto *objectContext = GetOwnerObjectContext();
        auto *transform = objectContext ? objectContext->GetComponent<Transform>() : nullptr;
        return transform ? transform->GetWorldMatrix() : Matrix4x4::Identity();
    }

    //==================================================
    // Renderer専用: GPUスキニング用リソース取得（Passkey<Renderer>限定）
    //==================================================

    /// @brief スキニング済み頂点数を取得
    std::uint32_t GetVertexCount(Passkey<Renderer>) const noexcept { return vertexCount_; }
    /// @brief スキニング元頂点（バインドポーズ）の構造化バッファを取得
    StructuredBufferResource *GetSourceVerticesBuffer(Passkey<Renderer>) const noexcept { return sourceVerticesBuffer_.get(); }
    /// @brief 頂点ごとのボーンインデックス・ウェイトの構造化バッファを取得
    StructuredBufferResource *GetSkinWeightsBuffer(Passkey<Renderer>) const noexcept { return skinWeightsBuffer_.get(); }
    /// @brief 現フレームのボーン行列パレットの構造化バッファを取得
    StructuredBufferResource *GetBoneMatricesBuffer(Passkey<Renderer>) const noexcept { return boneMatricesBuffer_.get(); }
    /// @brief スキニング用の定数バッファを取得
    ConstantBufferResource *GetSkinningConstantBuffer(Passkey<Renderer>) const noexcept { return skinningConstants_.get(); }
    /// @brief スキニング結果の書き込み先（UAV→VBVへ切り替えて使用）バッファを取得
    RWStructuredBufferResource *GetSkinnedVertexBuffer(Passkey<Renderer>) const noexcept { return skinnedVertexBuffer_.get(); }
    /// @brief BlendShape頂点差分（全BlendShape分を連結したもの）の構造化バッファを取得
    StructuredBufferResource *GetBlendShapeDeltasBuffer(Passkey<Renderer>) const noexcept { return blendShapeDeltasBuffer_.get(); }
    /// @brief BlendShapeごとの現在のウェイト（0～1に正規化前、0～100のまま）の構造化バッファを取得
    StructuredBufferResource *GetBlendShapeWeightsBuffer(Passkey<Renderer>) const noexcept { return blendShapeWeightsBuffer_.get(); }
    /// @brief スキニングに必要なデータが全て揃っているか（揃っていない場合は描画自体をスキップする）
    bool HasValidSkinningData(Passkey<Renderer>) const noexcept { return HasValidSkinningData(); }
    /// @brief 直接呼び出し版（SceneRenderer::BuildSortedDrawListから使用）
    RWStructuredBufferResource *GetSkinnedVertexBuffer() const noexcept { return skinnedVertexBuffer_.get(); }
    bool HasValidSkinningData() const noexcept {
        // ボーンを持たないメッシュ（jointNames_が空）でも、バインドポーズのまま
        // （+BlendShapeがあれば適用した状態で）描画できるようにする
        return vertexCount_ > 0 && sourceVerticesBuffer_ && skinWeightsBuffer_ &&
            boneMatricesBuffer_ && skinningConstants_ && skinnedVertexBuffer_ &&
            blendShapeDeltasBuffer_ && blendShapeWeightsBuffer_;
    }

#if defined(USE_IMGUI)
    /// @brief デバッグ描画用のジョイント情報（ワールド座標と親ジョイントのインデックス）
    struct DebugJointInfo {
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        int32_t parentIndex = -1;
    };
    /// @brief デバッグ描画用に、このコンポーネントのスケルトンインスタンスの
    ///        全ジョイントの現在のワールド座標を取得する（ボーンの線・球表示用）
    std::vector<DebugJointInfo> GetDebugJointInfos();
#endif

    /// @brief GPUスキニング用リソースを最新の状態へ更新する（メッシュ解決・静的バッファ構築・
    ///        現在のスケルトン姿勢からのボーン行列パレット/BlendShapeウェイトのアップロード）
    /// @details ゲームループが停止/一時停止中でも描画自体は継続されるため、Update()ではなく
    ///          Rendererから毎フレーム直接呼ばれる（CameraRenderer::RefreshConstantBufferと同じ考え方）。
    ///          アニメーションの時間経過（AdvanceAnimation）はUpdate()側でのみ行われる。
    void RefreshSkinningResources(Passkey<Renderer>) {
        RebuildSkinningResourcesIfNeeded();
        UpdateSkinningBuffers();
    }

protected:
    void Initialize() override;
    void Finalize() override;
    void Update() override;

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

    JSON SaveToJson() const override;
    bool LoadFromJson(const JSON &json) override;

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

    /// @brief 現在のメッシュハンドルに応じてスキニング用の静的リソース（元頂点・ボーンウェイト・
    ///        スキニング結果出力バッファ）を（再）構築する。メッシュが変わっていない場合は何もしない
    void RebuildSkinningResourcesIfNeeded();
    /// @brief シーン上のアーマチュア用オブジェクトへ現在のジョイント姿勢を同期する
    /// @details Root Boneオブジェクト（未設定の場合は所有オブジェクトの最上位の祖先）の子孫から
    ///          ジョイントと同名のオブジェクトを探し、そのTransformへジョイントのローカルTRSを
    ///          書き込む。モデルのプレハブが生成したアーマチュア階層がアニメーションに追従し、
    ///          ボーンへのオブジェクトのアタッチ等が行えるようになる
    void SyncArmatureObjects();
    /// @brief 選択中アニメーションを進行させ、スケルトンのジョイントへ反映する
    void AdvanceAnimation(float deltaTime);
    /// @brief 現在のスケルトンのジョイント姿勢からボーン行列パレット・定数バッファを更新する
    void UpdateSkinningBuffers();

    UUID128 targetObjectID_{};
    /// @brief Root Boneオブジェクト（設定時はこのオブジェクトのTransformに沿ってメッシュが動く）
    UUID128 rootBoneObjectID_{};
    std::string pipelineName_ = "Object3D.Solid.BlendNormal";
    /// @brief マテリアルスロット（サブメッシュごとのマテリアル名。常に1要素以上）
    std::vector<std::string> materialNames_{ "Default" };
    /// @brief materialNames_と対応するハンドルのキャッシュ（未解決はkInvalidHandle）
    mutable std::vector<MaterialManager::MaterialHandle> materialHandles_{ MaterialManager::kInvalidHandle };
    std::unordered_set<std::string> excludedRenderTargetNames_;

    SkinQuality quality_ = SkinQuality::Auto;
    std::vector<BlendShapeWeight> blendShapes_;
    std::string animationClipName_;
    bool playOnStart_ = true;
    bool loop_ = true;
    float playbackSpeed_ = 1.0f;

    // 再生状態（非シリアライズ）
    bool isPlaying_ = false;
    float elapsedTime_ = 0.0f;

    // メッシュ・スケルトン・アニメーションの解決結果（非シリアライズ、キャッシュ用）
    ModelManager::ModelHandle lastMeshHandle_ = ModelManager::kInvalidHandle;
    SkeletonManager::SkeletonHandle skeletonHandle_ = SkeletonManager::kInvalidHandle;
    AnimationManager::AnimationHandle animationHandle_ = AnimationManager::kInvalidHandle;
    std::uint32_t vertexCount_ = 0;
    /// @brief GPUのボーン行列パレットと対応するジョイント名の並び順
    std::vector<std::string> jointNames_;
    /// @brief jointNames_と対応するバインドポーズ逆行列
    std::vector<Matrix4x4> inverseBindPoses_;
    /// @brief このコンポーネント専用のスケルトンインスタンス（SkeletonManagerが保持する
    ///        アセット本体とは独立した複製）。同じスケルトンアセットを参照する複数の
    ///        SkinnedMeshRendererが互いのアニメーション再生状態（ジョイント姿勢）に
    ///        干渉しないよう、メッシュ解決時にSkeletonManager::CloneSkeletonで複製して保持する
    Skeleton skeletonInstance_;

    // GPUリソース（コンポーネント自身が所有・管理する）
    std::unique_ptr<StructuredBufferResource> sourceVerticesBuffer_;
    std::unique_ptr<StructuredBufferResource> skinWeightsBuffer_;
    std::unique_ptr<StructuredBufferResource> boneMatricesBuffer_;
    std::unique_ptr<ConstantBufferResource> skinningConstants_;
    std::unique_ptr<RWStructuredBufferResource> skinnedVertexBuffer_;
    /// @brief BlendShapeの頂点差分バッファ（[shapeIndex * vertexCount_ + vertexIndex]で参照する連結バッファ）
    std::unique_ptr<StructuredBufferResource> blendShapeDeltasBuffer_;
    /// @brief BlendShapeごとのウェイト（blendShapes_と同じ並び順）。ウェイトは毎フレーム更新する
    std::unique_ptr<StructuredBufferResource> blendShapeWeightsBuffer_;
};

REGISTER_COMPONENT_OBJECT(SkinnedMeshRenderer)

} // namespace KashipanEngine
