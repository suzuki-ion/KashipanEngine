#include "SkinnedMeshRenderer.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"

#if defined(USE_IMGUI)
#include <imgui.h>
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
    auto *sceneRenderer = GetOrAddSceneRenderer();
    if (sceneRenderer) {
        sceneRenderer->RegisterSkinnedMeshRenderer(this);
    }
    RebuildSkinningResourcesIfNeeded();
}

void SkinnedMeshRenderer::Finalize() {
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
    if (sceneRenderer) {
        sceneRenderer->UnregisterSkinnedMeshRenderer(this);
    }
}

void SkinnedMeshRenderer::RebuildSkinningResourcesIfNeeded() {
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

    // BlendShape一覧をメッシュの内容に同期する（既存のウェイトは名前が一致すれば引き継ぐ）
    const auto &modelBlendShapes = modelData.GetBlendShapes();
    {
        std::vector<BlendShapeWeight> newBlendShapes;
        newBlendShapes.reserve(modelBlendShapes.size());
        for (const auto &shape : modelBlendShapes) {
            float existingWeight = 0.0f;
            for (const auto &old : blendShapes_) {
                if (old.name == shape.name) {
                    existingWeight = old.weight;
                    break;
                }
            }
            newBlendShapes.push_back(BlendShapeWeight{ shape.name, existingWeight });
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

    // BlendShapeのウェイトは毎フレーム最新の値をアップロードする（ImGui等でいつでも変更され得るため）
    if (blendShapeWeightsBuffer_) {
        if (!blendShapes_.empty()) {
            std::vector<float> weights(blendShapes_.size());
            for (std::size_t i = 0; i < blendShapes_.size(); ++i) {
                weights[i] = blendShapes_[i].weight;
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
    // BlendShape一覧はメッシュ側から自動的に同期されるため、存在する名前のウェイトのみ更新する
    for (auto &bs : blendShapes_) {
        if (bs.name == name) {
            bs.weight = weight;
            return;
        }
    }
}

float SkinnedMeshRenderer::GetBlendShapeWeight(const std::string &name) const {
    for (const auto &bs : blendShapes_) {
        if (bs.name == name) return bs.weight;
    }
    return 0.0f;
}

#if defined(USE_IMGUI)
std::vector<SkinnedMeshRenderer::DebugJointInfo> SkinnedMeshRenderer::GetDebugJointInfos() {
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
    TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);
    TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
    ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames());
    ImGui::Checkbox("Cast Shadows", &castShadows_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("有効にすると、このメッシュがシャドウマッピングのシャドウキャスターとして扱われる");
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
    ImGui::Text("Skinning");
    static const char *kQualityNames[] = { "Auto", "Bone1", "Bone2", "Bone4" };
    int qualityIndex = static_cast<int>(quality_);
    if (ImGui::Combo("Quality", &qualityIndex, kQualityNames, IM_ARRAYSIZE(kQualityNames))) {
        quality_ = static_cast<SkinQuality>(qualityIndex);
    }
    if (!GetAnimator()) {
        ImGui::TextDisabled("(No Animator on this object: bind pose only)");
    }
    ImGui::Text("Vertex Count: %u", vertexCount_);
    ImGui::Text("Joint Count: %d", static_cast<int>(jointNames_.size()));

    if (ImGui::TreeNode("Blend Shapes")) {
        if (blendShapes_.empty()) {
            ImGui::TextDisabled("(No blend shapes on this mesh)");
        }
        // 一覧はメッシュ（インポートされたBlendShape）から自動的に同期されるため、
        // ここではウェイトの編集のみ行う（Unityと同じ挙動）
        for (std::size_t i = 0; i < blendShapes_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::DragFloat(blendShapes_[i].name.c_str(), &blendShapes_[i].weight, 0.1f, 0.0f, 100.0f);
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}
#endif

JSON SkinnedMeshRenderer::SaveToJson() const {
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
