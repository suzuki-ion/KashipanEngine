#include "SkinnedMeshRenderer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Debug/Logger.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Objects/Components/Render/PipelineVariantBuilder.h"
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

namespace {

/// @brief スキニング元/先の頂点データ（ResourceContainer::MeshVertexと同一レイアウト）
#pragma pack(push, 4)
struct SkinCPUVertex {
    Vector4 position{ 0.0f, 0.0f, 0.0f, 1.0f };
    Vector2 texcoord{ 0.0f, 0.0f };
    Vector3 normal{ 0.0f, 0.0f, 0.0f };
    Vector3 tangent{ 1.0f, 0.0f, 0.0f };
};
/// @brief 頂点ごとのボーンインデックス・ウェイト（HLSL側 SkinWeight と同一レイアウト）
struct SkinCPUWeight {
    std::uint32_t boneIndices[4]{ 0, 0, 0, 0 };
    float boneWeights[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
};
/// @brief Skinning.hlsl の SkinningConstants と同一レイアウト
struct SkinningConstantsCPU {
    std::uint32_t vertexCount = 0;
    std::uint32_t maxBonesPerVertex = 4;
    std::uint32_t blendShapeCount = 0;
    std::uint32_t padding = 0;
};
/// @brief BlendShape頂点差分1要素分（HLSL側 BlendShapeDelta と同一レイアウト）
struct BlendShapeDeltaCPU {
    Vector3 deltaPosition{ 0.0f, 0.0f, 0.0f };
    Vector3 deltaNormal{ 0.0f, 0.0f, 0.0f };
};
#pragma pack(pop)

} // namespace

void SkinnedMeshRenderer::Initialize() {
    LogScope scope;
    auto *sceneRenderer = GetOrAddSceneRenderer();
    if (sceneRenderer) {
        sceneRenderer->RegisterSkinnedMeshRenderer(this);
    }
    RebuildSkinningResourcesIfNeeded();
}

void SkinnedMeshRenderer::Finalize() {
    LogScope scope;
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
    if (sceneRenderer) {
        sceneRenderer->UnregisterSkinnedMeshRenderer(this);
    }
}

void SkinnedMeshRenderer::RebuildSkinningResourcesIfNeeded() {
    LogScope scope;
    const auto meshHandle = GetMeshHandle();
    if (meshHandle == ModelManager::kInvalidHandle) return;
    if (meshHandle == lastMeshHandle_ && sourceVerticesBuffer_) return;

    lastMeshHandle_ = meshHandle;
    const auto &modelData = ModelManager::GetModelData(meshHandle);

    // ボーンを持たないメッシュ（AnimatedCubeのような単なるノードアニメーションのモデル等）でも、
    // バインドポーズのまま描画できるよう、頂点さえあれば以降の処理を続行する
    // （jointNames_が空の場合、頂点ごとのボーンウェイトは全て0になりバインドポーズのまま出力される）
    if (modelData.GetVertexCount() == 0) {
        vertexCount_ = 0;
        jointNames_.clear();
        inverseBindPoses_.clear();
        blendShapes_.clear();
        blendShapeNameToModelIndex_.clear();
        sourceVerticesBuffer_.reset();
        skinWeightsBuffer_.reset();
        boneMatricesBuffer_.reset();
        skinnedVertexBuffer_.reset();
        skinningConstants_.reset();
        blendShapeDeltasBuffer_.reset();
        blendShapeWeightsBuffer_.reset();
        return;
    }

    vertexCount_ = modelData.GetVertexCount();

    // 元頂点（バインドポーズ）バッファ
    std::vector<SkinCPUVertex> cpuVertices(vertexCount_);
    const auto &srcVertices = modelData.GetVertices();
    for (std::uint32_t i = 0; i < vertexCount_; ++i) {
        const auto &src = srcVertices[i];
        cpuVertices[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
        cpuVertices[i].texcoord = Vector2(src.u, src.v);
        cpuVertices[i].normal = Vector3(src.nx, src.ny, src.nz);
        cpuVertices[i].tangent = Vector3(src.tx, src.ty, src.tz);
    }
    sourceVerticesBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(SkinCPUVertex), vertexCount_, cpuVertices.data());

    // ジョイント一覧・逆バインドポーズ行列を確定し、頂点ごとの上位4ボーンを抽出する
    jointNames_.clear();
    inverseBindPoses_.clear();
    std::vector<std::vector<std::pair<float, std::uint32_t>>> perVertex(vertexCount_);
    for (const auto &cluster : modelData.GetSkinClusters()) {
        const std::uint32_t jointIndex = static_cast<std::uint32_t>(jointNames_.size());
        jointNames_.push_back(cluster.first);
        inverseBindPoses_.push_back(cluster.second.inverseBindPoseMatrix);
        for (const auto &vw : cluster.second.vertexWeights) {
            if (vw.vertexIndex >= vertexCount_) continue;
            perVertex[vw.vertexIndex].emplace_back(vw.weight, jointIndex);
        }
    }

    std::vector<SkinCPUWeight> cpuWeights(vertexCount_);
    for (std::uint32_t v = 0; v < vertexCount_; ++v) {
        auto &list = perVertex[v];
        std::sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        if (list.size() > 4) list.resize(4);
        float total = 0.0f;
        for (const auto &pr : list) total += pr.first;
        for (std::size_t i = 0; i < 4; ++i) {
            if (i < list.size() && total > 0.0f) {
                cpuWeights[v].boneIndices[i] = list[i].second;
                cpuWeights[v].boneWeights[i] = list[i].first / total;
            }
        }
    }
    skinWeightsBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(SkinCPUWeight), vertexCount_, cpuWeights.data());

    const std::size_t jointCount = std::max<std::size_t>(1, jointNames_.size());
    std::vector<Matrix4x4> identityMatrices(jointCount, Matrix4x4::Identity());
    boneMatricesBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(Matrix4x4), jointCount, identityMatrices.data());

    skinningConstants_ = std::make_unique<ConstantBufferResource>(sizeof(SkinningConstantsCPU));
    skinnedVertexBuffer_ = std::make_unique<RWStructuredBufferResource>(sizeof(SkinCPUVertex), vertexCount_);

    // BlendShape名からGPUバッファ上のインデックス（=ModelData::GetBlendShapes()の並び順）への対応表を
    // 作り直す。ウェイトアップロード（UpdateSkinningBuffers）はこの表を使って名前引きするため、
    // blendShapes_（Inspector表示順）を自由に並べ替えても実際に適用されるBlendShapeはズレない
    const auto &modelBlendShapes = modelData.GetBlendShapes();
    blendShapeNameToModelIndex_.clear();
    blendShapeNameToModelIndex_.reserve(modelBlendShapes.size());
    for (std::size_t i = 0; i < modelBlendShapes.size(); ++i) {
        blendShapeNameToModelIndex_[modelBlendShapes[i].name] = static_cast<std::uint32_t>(i);
    }

    // BlendShape一覧をメッシュの内容に同期する。FBX（Blenderのシェイプキー順）とAssimpの
    // インポート順が一致しないことがあり、ユーザーがInspector上で並べ替えて修正できるようにしているため、
    // 既存の表示順（blendShapes_。ロード直後ならJSONから復元された順）はモデルに現存する限り保ち、
    // モデル側に新しく増えたBlendShapeのみ末尾に追加する（ウェイトは名前が一致すれば引き継ぐ）
    {
        std::vector<BlendShapeWeight> newBlendShapes;
        newBlendShapes.reserve(modelBlendShapes.size());
        std::unordered_set<std::string> added;
        added.reserve(modelBlendShapes.size());
        for (const auto &old : blendShapes_) {
            if (blendShapeNameToModelIndex_.count(old.name) && added.insert(old.name).second) {
                newBlendShapes.push_back(old);
            }
        }
        for (const auto &shape : modelBlendShapes) {
            if (added.insert(shape.name).second) {
                newBlendShapes.push_back(BlendShapeWeight{ shape.name, 0.0f });
            }
        }
        blendShapes_ = std::move(newBlendShapes);
    }

    // BlendShape頂点差分の連結バッファ（[shapeIndex * vertexCount_ + vertexIndex]で参照する）
    const std::size_t blendShapeCount = modelBlendShapes.size();
    const std::size_t deltaElementCount = std::max<std::size_t>(1, blendShapeCount * static_cast<std::size_t>(vertexCount_));
    std::vector<BlendShapeDeltaCPU> cpuDeltas(deltaElementCount);
    for (std::size_t shapeIndex = 0; shapeIndex < modelBlendShapes.size(); ++shapeIndex) {
        for (const auto &delta : modelBlendShapes[shapeIndex].deltas) {
            if (delta.vertexIndex >= vertexCount_) continue;
            auto &dstDelta = cpuDeltas[shapeIndex * static_cast<std::size_t>(vertexCount_) + delta.vertexIndex];
            dstDelta.deltaPosition = delta.deltaPosition;
            dstDelta.deltaNormal = delta.deltaNormal;
        }
    }
    blendShapeDeltasBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(BlendShapeDeltaCPU), deltaElementCount, cpuDeltas.data());

    const std::size_t weightElementCount = std::max<std::size_t>(1, blendShapeCount);
    blendShapeWeightsBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(float), weightElementCount);

    UpdateSkinningBuffers();
}

void SkinnedMeshRenderer::UpdateSkinningBuffers() {
    LogScope scope;
    if (!boneMatricesBuffer_ || !skinningConstants_) return;

    // jointNames_が空（ボーンを持たないメッシュ）の場合でも、gVertexCount等の定数と
    // BlendShapeウェイトは毎フレーム正しくアップロードする必要がある
    std::vector<Matrix4x4> boneMatrices(std::max<std::size_t>(1, jointNames_.size()), Matrix4x4::Identity());

    // Animatorが保持するスケルトンインスタンスから現在の姿勢を読み取る（Animatorが無い場合、
    // またはジョイント名が一致しない場合はバインドポーズ相当のIdentityのままになる）
    if (!jointNames_.empty()) {
        if (auto *animator = GetAnimator()) {
            const Skeleton &skeleton = animator->GetSkeletonInstance(Passkey<SkinnedMeshRenderer>{});
            for (std::size_t i = 0; i < jointNames_.size(); ++i) {
                auto it = skeleton.jointNameToIndexMap.find(jointNames_[i]);
                if (it == skeleton.jointNameToIndexMap.end()) continue;
                SkeletonTransform *transform = skeleton.joints[static_cast<std::size_t>(it->second)].transform.get();
                if (!transform) continue;
                boneMatrices[i] = inverseBindPoses_[i] * transform->GetWorldMatrix();
            }
        }
    }

    if (auto *mapped = static_cast<Matrix4x4 *>(boneMatricesBuffer_->Map())) {
        std::memcpy(mapped, boneMatrices.data(), sizeof(Matrix4x4) * boneMatrices.size());
    }

    SkinningConstantsCPU constants{};
    constants.vertexCount = vertexCount_;
    switch (quality_) {
        case SkinQuality::Bone1: constants.maxBonesPerVertex = 1; break;
        case SkinQuality::Bone2: constants.maxBonesPerVertex = 2; break;
        case SkinQuality::Bone4: constants.maxBonesPerVertex = 4; break;
        case SkinQuality::Auto:
        default: constants.maxBonesPerVertex = 4; break;
    }
    constants.blendShapeCount = static_cast<std::uint32_t>(blendShapes_.size());
    if (auto *mapped = skinningConstants_->Map()) {
        std::memcpy(mapped, &constants, sizeof(constants));
    }

    // BlendShapeのウェイトは毎フレーム最新の値をアップロードする（ImGui等でいつでも変更され得るため）。
    // blendShapes_はInspector上の表示順（並べ替え済みの場合がある）なので、そのまま位置で
    // 書き込まずblendShapeNameToModelIndex_で名前引きし、GPU側（ModelData順）の正しいスロットへ書く
    if (blendShapeWeightsBuffer_) {
        if (!blendShapes_.empty()) {
            std::vector<float> weights(blendShapes_.size(), 0.0f);
            for (const auto &bs : blendShapes_) {
                auto it = blendShapeNameToModelIndex_.find(bs.name);
                if (it != blendShapeNameToModelIndex_.end() && it->second < weights.size()) {
                    weights[it->second] = bs.weight;
                }
            }
            if (auto *mapped = static_cast<float *>(blendShapeWeightsBuffer_->Map())) {
                std::memcpy(mapped, weights.data(), sizeof(float) * weights.size());
            }
        } else if (auto *mapped = static_cast<float *>(blendShapeWeightsBuffer_->Map())) {
            const float zero = 0.0f;
            std::memcpy(mapped, &zero, sizeof(zero));
        }
    }
}

void SkinnedMeshRenderer::SetBlendShapeWeight(const std::string &name, float weight) {
    LogScope scope;
    // BlendShape一覧はメッシュ側から自動的に同期されるため、存在する名前のウェイトのみ更新する
    for (auto &bs : blendShapes_) {
        if (bs.name == name) {
            bs.weight = weight;
            return;
        }
    }
}

float SkinnedMeshRenderer::GetBlendShapeWeight(const std::string &name) const {
    LogScope scope;
    for (const auto &bs : blendShapes_) {
        if (bs.name == name) return bs.weight;
    }
    return 0.0f;
}

#if defined(USE_IMGUI)
std::vector<SkinnedMeshRenderer::DebugJointInfo> SkinnedMeshRenderer::GetDebugJointInfos() {
    LogScope scope;
    std::vector<DebugJointInfo> result;
    auto *animator = GetAnimator();
    if (!animator) return result;
    const Skeleton &skeleton = animator->GetSkeletonInstance(Passkey<SkinnedMeshRenderer>{});
    if (skeleton.joints.empty()) return result;

    // ジョイントのスケルトン空間行列に、描画に使うワールド行列（Root Bone考慮済み）を掛けて
    // シーン上のワールド座標を得る
    const Matrix4x4 world = GetWorldMatrix();
    result.reserve(skeleton.joints.size());
    for (auto &joint : skeleton.joints) {
        DebugJointInfo info;
        if (joint.transform) {
            const Matrix4x4 m = joint.transform->GetWorldMatrix() * world;
            info.position = Vector3(m.m[3][0], m.m[3][1], m.m[3][2]);
        }
        info.parentIndex = joint.parentIndex.value_or(-1);
        result.push_back(info);
    }
    return result;
}

void SkinnedMeshRenderer::ShowImGui() {
    LogScope scope;
    TargetObjectSelector::ShowSelector(TranslationLabel("component.common.target"), GetOwnerSceneContext(), targetObjectID_);
    TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
    ImGuiCustom::SelectString(TranslationLabel("component.skinnedmeshrenderer.pipeline"), pipelineName_, PipelineManager::GetLoadedRenderPipelineNames("3D"));
    // カスタムバリアントビルダー（MeshRendererと共通のUI。実装はPipelineVariantBuilder参照）。
    // SkinnedMeshRendererの描画エントリは毎フレーム収集し直されるため、MeshRendererと異なり
    // 明示的な再構築通知（MarkDrawListDirty相当）は不要
    PipelineVariantBuilder::Show(pipelineName_);
    ImGui::Checkbox(TranslationLabel("component.skinnedmeshrenderer.cast_shadows"), &castShadows_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.skinnedmeshrenderer.desc_1"));
    }

    ImGui::ColorEdit4(TranslationLabel("component.skinnedmeshrenderer.instance_color"), &instanceColor_.x);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.skinnedmeshrenderer.desc_2"));
    }
    const char *kColorBlendModeLabels[] = { TranslationC("component.common.blendmode.override"), TranslationC("component.common.blendmode.multiply"), TranslationC("component.common.blendmode.add"), TranslationC("component.common.blendmode.subtract") };
    int blendModeIndex = static_cast<int>(instanceColorBlendMode_);
    if (ImGui::Combo(TranslationLabel("component.skinnedmeshrenderer.instance_color_blend_mode"), &blendModeIndex, kColorBlendModeLabels, IM_ARRAYSIZE(kColorBlendModeLabels))) {
        instanceColorBlendMode_ = static_cast<ColorBlendMode>(blendModeIndex);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.skinnedmeshrenderer.desc_3"));
    }

    ImGui::TextUnformatted(TranslationC("component.common.instance_uv_transform"));
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.common.desc_instance_uv"));
    }
    ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_translate"), &instanceUvTranslate_.x, 0.001f);
    float instanceUvRotationDeg = instanceUvRotation_ * 180.0f / 3.14159265f;
    if (ImGui::DragFloat(TranslationLabel("component.common.instance_uv_rotation"), &instanceUvRotationDeg, 0.1f, -180.0f, 180.0f)) {
        instanceUvRotation_ = instanceUvRotationDeg * 3.14159265f / 180.0f;
    }
    ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_scale"), &instanceUvScale_.x, 0.001f);
    ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_pivot"), &instanceUvPivot_.x, 0.001f);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.common.desc_instance_uv_pivot"));
    }
    const char *kUvCombineModeLabels[] = {
        TranslationC("component.common.uvcombinemode.material_then_instance"),
        TranslationC("component.common.uvcombinemode.instance_then_material"),
        TranslationC("component.common.uvcombinemode.instance_only"),
    };
    int uvCombineModeIndex = static_cast<int>(instanceUvCombineMode_);
    if (ImGui::Combo(TranslationLabel("component.common.instance_uv_combine_mode"), &uvCombineModeIndex, kUvCombineModeLabels, IM_ARRAYSIZE(kUvCombineModeLabels))) {
        instanceUvCombineMode_ = static_cast<UVCombineMode>(uvCombineModeIndex);
    }
    // ピクセル基準（0～テクスチャの幅・高さ）でのインスタンスUV編集。内部値（0～1のUV基準）と相互に連動する
    {
        auto *materialForUv = MaterialManager::GetMaterial(GetMaterialHandle());
        const auto textureView = TextureManager::GetTextureView(materialForUv ? materialForUv->textureHandle : TextureManager::kInvalidHandle);
        const float texWidth = static_cast<float>(textureView.GetWidth());
        const float texHeight = static_cast<float>(textureView.GetHeight());
        const bool hasTextureSize = texWidth > 0.0f && texHeight > 0.0f;

        ImGui::BeginDisabled(!hasTextureSize);
        ImGui::TextUnformatted(TranslationC("component.common.instance_uv_transform_pixel"));
        Vector2 pxTranslate = hasTextureSize
            ? Vector2(instanceUvTranslate_.x * texWidth, instanceUvTranslate_.y * texHeight) : Vector2::Zero();
        if (ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_translate_pixel"), &pxTranslate.x, 0.5f) && hasTextureSize) {
            instanceUvTranslate_ = Vector2(pxTranslate.x / texWidth, pxTranslate.y / texHeight);
        }
        Vector2 pxScale = hasTextureSize
            ? Vector2(instanceUvScale_.x * texWidth, instanceUvScale_.y * texHeight) : Vector2::Zero();
        if (ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_scale_pixel"), &pxScale.x, 0.5f) && hasTextureSize) {
            instanceUvScale_ = Vector2(pxScale.x / texWidth, pxScale.y / texHeight);
        }
        Vector2 pxPivot = hasTextureSize
            ? Vector2(instanceUvPivot_.x * texWidth, instanceUvPivot_.y * texHeight) : Vector2::Zero();
        if (ImGui::DragFloat2(TranslationLabel("component.common.instance_uv_pivot_pixel"), &pxPivot.x, 0.5f) && hasTextureSize) {
            instanceUvPivot_ = Vector2(pxPivot.x / texWidth, pxPivot.y / texHeight);
        }
        ImGui::EndDisabled();
    }

    ImGui::DragInt(TranslationLabel("component.common.render_priority"), &renderPriority_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.common.desc_render_priority"));
    }
    ImGui::Checkbox(TranslationLabel("component.common.allow_instancing"), &allowInstancing_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TranslationC("component.common.desc_allow_instancing"));
    }

    const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
    std::vector<std::string> materialNames;
    for (const auto &entry : materialEntries) {
        materialNames.push_back(entry.material.name);
    }

    // マテリアルスロット（メッシュのサブメッシュ数に合わせて表示する）
    const auto meshHandleForSlots = GetMeshHandle();
    if (meshHandleForSlots != ModelManager::kInvalidHandle) {
        const size_t subMeshCount = std::max<size_t>(1, ModelManager::GetModelData(meshHandleForSlots).GetSubMeshes().size());
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
        }
        if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
            for (const auto &entry : materialEntries) {
                if (entry.assetPath == droppedPath) {
                    materialNames_[i] = entry.material.name;
                    materialHandles_[i] = MaterialManager::kInvalidHandle;
                    break;
                }
            }
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text("%s", TranslationC("component.skinnedmeshrenderer.skinning"));
    static const char *kQualityNames[] = { "Auto", "Bone1", "Bone2", "Bone4" };
    int qualityIndex = static_cast<int>(quality_);
    if (ImGui::Combo(TranslationLabel("component.skinnedmeshrenderer.quality"), &qualityIndex, kQualityNames, IM_ARRAYSIZE(kQualityNames))) {
        quality_ = static_cast<SkinQuality>(qualityIndex);
    }
    if (!GetAnimator()) {
        ImGui::TextDisabled("%s", TranslationC("component.skinnedmeshrenderer.no_animator_on_this_object_bind_pose_only"));
    }
    ImGui::Text(TranslationC("component.skinnedmeshrenderer.vertex_count_u"), vertexCount_);
    ImGui::Text(TranslationC("component.skinnedmeshrenderer.joint_count_d"), static_cast<int>(jointNames_.size()));

    if (ImGui::TreeNode(TranslationLabel("component.skinnedmeshrenderer.blend_shapes"))) {
        if (blendShapes_.empty()) {
            ImGui::TextDisabled("%s", TranslationC("component.skinnedmeshrenderer.no_blend_shapes_on_this_mesh"));
        }
        // 一覧はメッシュ（インポートされたBlendShape）から自動的に同期されるため、追加・削除はできない
        // （Unityと同じ挙動）。ただしFBX（Blenderのシェイプキー順）とAssimpのインポート順が
        // 一致しないことがあるため、↑↓ボタンで表示順だけは並べ替えられるようにしている
        // （実際に適用されるBlendShapeはblendShapeNameToModelIndex_の名前引きで対応付くため、
        //   並べ替えても挙動は変わらない）
        for (std::size_t i = 0; i < blendShapes_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginDisabled(i == 0);
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                std::swap(blendShapes_[i], blendShapes_[i - 1]);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 >= blendShapes_.size());
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
                std::swap(blendShapes_[i], blendShapes_[i + 1]);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::DragFloat(blendShapes_[i].name.c_str(), &blendShapes_[i].weight, 0.1f, 0.0f, 100.0f);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}
#endif

JSON SkinnedMeshRenderer::SaveToJson() const {
    LogScope scope;
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
    json["instanceUvTranslate"] = ToJSON(instanceUvTranslate_);
    json["instanceUvRotation"] = instanceUvRotation_;
    json["instanceUvScale"] = ToJSON(instanceUvScale_);
    json["instanceUvPivot"] = ToJSON(instanceUvPivot_);
    json["instanceUvCombineMode"] = static_cast<int>(instanceUvCombineMode_);
    json["renderPriority"] = renderPriority_;
    json["allowInstancing"] = allowInstancing_;
    json["quality"] = static_cast<int>(quality_);
    JSON blendShapesJson = JSON::array();
    for (const auto &bs : blendShapes_) {
        JSON bsj = JSON::object();
        bsj["name"] = bs.name;
        bsj["weight"] = bs.weight;
        blendShapesJson.push_back(bsj);
    }
    json["blendShapes"] = blendShapesJson;
    return json;
}

bool SkinnedMeshRenderer::LoadFromJson(const JSON &json) {
    LogScope scope;
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
    instanceUvTranslate_ = json.contains("instanceUvTranslate") ? FromJSON<Vector2>(json["instanceUvTranslate"]) : Vector2(0.0f, 0.0f);
    instanceUvRotation_ = json.value("instanceUvRotation", 0.0f);
    instanceUvScale_ = json.contains("instanceUvScale") ? FromJSON<Vector2>(json["instanceUvScale"]) : Vector2(1.0f, 1.0f);
    instanceUvPivot_ = json.contains("instanceUvPivot") ? FromJSON<Vector2>(json["instanceUvPivot"]) : Vector2(0.5f, 0.5f);
    instanceUvCombineMode_ = static_cast<UVCombineMode>(json.value("instanceUvCombineMode", static_cast<int>(UVCombineMode::MaterialThenInstance)));
    renderPriority_ = json.value("renderPriority", 0);
    allowInstancing_ = json.value("allowInstancing", true);
    quality_ = static_cast<SkinQuality>(json.value("quality", static_cast<int>(SkinQuality::Auto)));
    blendShapes_.clear();
    for (const auto &bsj : json.value("blendShapes", std::vector<JSON>())) {
        BlendShapeWeight bs;
        bs.name = bsj.value("name", std::string{});
        bs.weight = bsj.value("weight", 0.0f);
        blendShapes_.push_back(std::move(bs));
    }
    return true;
}

} // namespace KashipanEngine
