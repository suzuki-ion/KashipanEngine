#include "SkinnedMeshRenderer.h"

#include <algorithm>
#include <cstring>

#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RWStructuredBufferResource.h"
#include "Graphics/Resources/StructuredBufferResource.h"
#include "Math/Vector2.h"
#include "Math/Vector4.h"
#include "Utilities/TimeUtils.h"

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

float SampleFloatTimeline(const KeyframeTimeline &timeline, float time, bool loop) {
    if (timeline.keys.empty()) return 0.0f;
    if (timeline.keys.size() == 1) return std::get<float>(timeline.keys.front().value);

    const float lastKeyTime = timeline.keys.back().time;
    const float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return std::get<float>(timeline.keys.back().value);

    float sampledTime = time;
    if (loop) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const KeyframeNode &key) { return t < key.time; });
    if (upper == timeline.keys.begin()) return std::get<float>(timeline.keys.front().value);
    if (upper == timeline.keys.end()) return std::get<float>(timeline.keys.back().value);

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float span = to.time - from.time;
    const float n = span > 0.0f ? (sampledTime - from.time) / span : 0.0f;
    return std::get<float>(from.value) + (std::get<float>(to.value) - std::get<float>(from.value)) * n;
}

Quaternion SampleQuaternionTimeline(const KeyframeTimeline &timeline, float time, bool loop) {
    if (timeline.keys.empty()) return Quaternion::Identity();
    if (timeline.keys.size() == 1) return std::get<Quaternion>(timeline.keys.front().value);

    const float lastKeyTime = timeline.keys.back().time;
    const float endTime = timeline.duration > 0.0f ? timeline.duration : lastKeyTime;
    if (endTime <= 0.0f) return std::get<Quaternion>(timeline.keys.back().value);

    float sampledTime = time;
    if (loop) {
        sampledTime = std::fmod(sampledTime, endTime);
        if (sampledTime < 0.0f) sampledTime += endTime;
    } else {
        sampledTime = std::clamp(sampledTime, 0.0f, endTime);
    }

    auto upper = std::upper_bound(timeline.keys.begin(), timeline.keys.end(), sampledTime,
        [](float t, const KeyframeNode &key) { return t < key.time; });
    if (upper == timeline.keys.begin()) return std::get<Quaternion>(timeline.keys.front().value);
    if (upper == timeline.keys.end()) return std::get<Quaternion>(timeline.keys.back().value);

    const auto &to = *upper;
    const auto &from = *(upper - 1);
    const float span = to.time - from.time;
    const float n = span > 0.0f ? (sampledTime - from.time) / span : 0.0f;
    return Quaternion::Slerp(std::get<Quaternion>(from.value), std::get<Quaternion>(to.value), n);
}

/// @brief アニメーションクリップをスケルトンの各ジョイントのTransformへ適用する
/// @details KeyframeAnimator::PlayFromAnimationAndSkeletonHandle と同様の考え方だが、
///          SkinnedMeshRendererはシーンコンポーネントを経由せず自身のUpdateから直接評価する
void ApplyClipToSkeletonJoints(const AnimationClip &clip, const Skeleton &skeleton, float time, bool loop) {
    for (const auto &joint : skeleton.joints) {
        auto it = clip.nodeNameToTimelineIndices.find(joint.name);
        if (it == clip.nodeNameToTimelineIndices.end()) continue;

        SkeletonTransform *transform = joint.transform.get();
        if (!transform) continue;

        Vector3 translate = transform->GetTranslate();
        Vector3 scale = transform->GetScale();
        Quaternion rotate = transform->GetRotate();
        bool hasTranslate = false;
        bool hasScale = false;
        bool hasRotate = false;

        for (auto timelineIndex : it->second) {
            if (timelineIndex >= clip.timelines.size()) continue;
            const auto &timeline = clip.timelines[timelineIndex];
            if (timeline.valueType == KeyframeValueType::Float) {
                const float v = SampleFloatTimeline(timeline, time, loop);
                if (timeline.name.ends_with(".Translate.X")) { translate.x = v; hasTranslate = true; }
                else if (timeline.name.ends_with(".Translate.Y")) { translate.y = v; hasTranslate = true; }
                else if (timeline.name.ends_with(".Translate.Z")) { translate.z = v; hasTranslate = true; }
                else if (timeline.name.ends_with(".Scale.X")) { scale.x = v; hasScale = true; }
                else if (timeline.name.ends_with(".Scale.Y")) { scale.y = v; hasScale = true; }
                else if (timeline.name.ends_with(".Scale.Z")) { scale.z = v; hasScale = true; }
            } else if (timeline.valueType == KeyframeValueType::Quaternion && timeline.name.ends_with(".Rotate")) {
                rotate = SampleQuaternionTimeline(timeline, time, loop);
                hasRotate = true;
            }
        }

        if (hasTranslate) transform->SetTranslate(translate);
        if (hasScale) transform->SetScale(scale);
        if (hasRotate) transform->SetRotate(rotate);
    }
}

} // namespace

void SkinnedMeshRenderer::Initialize() {
    auto *sceneRenderer = GetOrAddSceneRenderer();
    if (sceneRenderer) {
        sceneRenderer->RegisterSkinnedMeshRenderer(this);
    }
    isPlaying_ = playOnStart_;
    RebuildSkinningResourcesIfNeeded();
}

void SkinnedMeshRenderer::Finalize() {
    auto *sceneContext = GetOwnerSceneContext();
    auto *sceneRenderer = sceneContext ? sceneContext->GetComponent<SceneRenderer>() : nullptr;
    if (sceneRenderer) {
        sceneRenderer->UnregisterSkinnedMeshRenderer(this);
    }
}

void SkinnedMeshRenderer::Update() {
    // メッシュ解決・GPUリソースの構築・アップロードはRefreshSkinningResources（Renderer側から毎フレーム
    // 呼ばれる）が担当する。ここではゲームループが動いている間だけ進めるべきアニメーション時間の
    // 経過のみを行う（ゲームループ停止/一時停止中はUpdate自体が呼ばれないため、その間は自動的に静止する）。
    if (vertexCount_ == 0) return;
    AdvanceAnimation(GetDeltaTime() * GetGameSpeed());
}

void SkinnedMeshRenderer::RebuildSkinningResourcesIfNeeded() {
    const auto meshHandle = GetMeshHandle();
    if (meshHandle == ModelManager::kInvalidHandle) return;
    if (meshHandle == lastMeshHandle_ && sourceVerticesBuffer_) return;

    lastMeshHandle_ = meshHandle;
    const auto &modelData = ModelManager::GetModelData(meshHandle);

    if (!modelData.HasSkinning() || modelData.GetVertexCount() == 0) {
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
    skeletonHandle_ = SkeletonManager::GetSkeletonHandleFromAssetPath(modelData.GetAssetRelativePath());
    animationHandle_ = AnimationManager::GetAnimationHandleFromAssetPath(modelData.GetAssetRelativePath());

    // 元頂点（バインドポーズ）バッファ
    std::vector<SkinCPUVertex> cpuVertices(vertexCount_);
    const auto &srcVertices = modelData.GetVertices();
    for (std::uint32_t i = 0; i < vertexCount_; ++i) {
        const auto &src = srcVertices[i];
        cpuVertices[i].position = Vector4(src.px, src.py, src.pz, 1.0f);
        cpuVertices[i].texcoord = Vector2(src.u, src.v);
        cpuVertices[i].normal = Vector3(src.nx, src.ny, src.nz);
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

void SkinnedMeshRenderer::AdvanceAnimation(float deltaTime) {
    if (!isPlaying_ || animationClipName_.empty()) return;
    if (skeletonHandle_ == SkeletonManager::kInvalidHandle || animationHandle_ == AnimationManager::kInvalidHandle) return;

    const auto &animData = AnimationManager::GetAnimationData(animationHandle_);
    const AnimationClip *clip = animData.FindClipByName(animationClipName_);
    if (!clip) return;

    elapsedTime_ += deltaTime * playbackSpeed_;

    const auto &skeletonData = SkeletonManager::GetSkeletonData(skeletonHandle_);
    ApplyClipToSkeletonJoints(*clip, skeletonData.GetSkeleton(), elapsedTime_, loop_);

    if (!loop_) {
        float clipEndTime = 0.0f;
        for (const auto &timeline : clip->timelines) {
            const float endTime = timeline.duration > 0.0f ? timeline.duration
                : (timeline.keys.empty() ? 0.0f : timeline.keys.back().time);
            clipEndTime = std::max(clipEndTime, endTime);
        }
        if (elapsedTime_ >= clipEndTime) {
            isPlaying_ = false;
        }
    }
}

void SkinnedMeshRenderer::UpdateSkinningBuffers() {
    if (!boneMatricesBuffer_ || !skinningConstants_ || jointNames_.empty()) return;

    std::vector<Matrix4x4> boneMatrices(jointNames_.size(), Matrix4x4::Identity());

    if (skeletonHandle_ != SkeletonManager::kInvalidHandle) {
        const auto &skeletonData = SkeletonManager::GetSkeletonData(skeletonHandle_);
        const Skeleton &skeleton = skeletonData.GetSkeleton();
        for (std::size_t i = 0; i < jointNames_.size(); ++i) {
            auto it = skeleton.jointNameToIndexMap.find(jointNames_[i]);
            if (it == skeleton.jointNameToIndexMap.end()) continue;
            SkeletonTransform *transform = skeleton.joints[static_cast<std::size_t>(it->second)].transform.get();
            if (!transform) continue;
            boneMatrices[i] = inverseBindPoses_[i] * transform->GetWorldMatrix();
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

std::vector<std::string> SkinnedMeshRenderer::GetAvailableAnimationClipNames() const {
    std::vector<std::string> names;
    const auto meshHandle = GetMeshHandle();
    if (meshHandle == ModelManager::kInvalidHandle) return names;
    const auto &modelData = ModelManager::GetModelData(meshHandle);
    const auto animHandle = AnimationManager::GetAnimationHandleFromAssetPath(modelData.GetAssetRelativePath());
    if (animHandle == AnimationManager::kInvalidHandle) return names;
    const auto &animData = AnimationManager::GetAnimationData(animHandle);
    for (const auto &clip : animData.GetClips()) {
        names.push_back(clip.name);
    }
    return names;
}

#if defined(USE_IMGUI)
void SkinnedMeshRenderer::ShowImGui() {
    TargetObjectSelector::ShowSelector("Target", GetOwnerSceneContext(), targetObjectID_);
    TargetObjectSelector::ShowRenderTargetFilters(GetOwnerSceneContext(), targetObjectID_, excludedRenderTargetNames_);
    ImGuiCustom::SelectString("Pipeline", pipelineName_, PipelineManager::GetLoadedRenderPipelineNames());
    const auto materialEntries = MaterialManager::GetLoadedMaterialListEntries();
    std::vector<std::string> materialNames;
    for (const auto &entry : materialEntries) {
        materialNames.push_back(entry.material.name);
    }
    if (ImGuiCustom::SelectString("Material", materialName_, materialNames)) {
        materialHandle_ = MaterialManager::kInvalidHandle;
    }
    if (std::string droppedPath; AcceptAssetDragDropTarget(kMaterialAssetDragDropType, droppedPath)) {
        for (const auto &entry : materialEntries) {
            if (entry.assetPath == droppedPath) {
                materialName_ = entry.material.name;
                materialHandle_ = MaterialManager::kInvalidHandle;
                break;
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Skinning");
    static const char *kQualityNames[] = { "Auto", "Bone1", "Bone2", "Bone4" };
    int qualityIndex = static_cast<int>(quality_);
    if (ImGui::Combo("Quality", &qualityIndex, kQualityNames, IM_ARRAYSIZE(kQualityNames))) {
        quality_ = static_cast<SkinQuality>(qualityIndex);
    }

    const auto clipNames = GetAvailableAnimationClipNames();
    if (ImGuiCustom::SelectString("Animation Clip", animationClipName_, clipNames, true)) {
        elapsedTime_ = 0.0f;
    }
    ImGui::Checkbox("Play On Start", &playOnStart_);
    ImGui::Checkbox("Loop", &loop_);
    ImGui::DragFloat("Playback Speed", &playbackSpeed_, 0.01f, 0.0f, 10.0f);
    if (ImGui::Button(isPlaying_ ? "Stop" : "Play")) {
        isPlaying_ = !isPlaying_;
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
    json["materialName"] = materialName_;
    for (const auto &name : excludedRenderTargetNames_) {
        json["excludedRenderTargetNames"].push_back(name);
    }
    json["quality"] = static_cast<int>(quality_);
    json["animationClipName"] = animationClipName_;
    json["playOnStart"] = playOnStart_;
    json["loop"] = loop_;
    json["playbackSpeed"] = playbackSpeed_;
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
    materialName_ = json.value("materialName", std::string{ "Default" });
    materialHandle_ = MaterialManager::kInvalidHandle;
    excludedRenderTargetNames_.clear();
    for (const auto &name : json.value("excludedRenderTargetNames", std::vector<std::string>())) {
        excludedRenderTargetNames_.insert(name);
    }
    quality_ = static_cast<SkinQuality>(json.value("quality", static_cast<int>(SkinQuality::Auto)));
    animationClipName_ = json.value("animationClipName", std::string{});
    playOnStart_ = json.value("playOnStart", true);
    loop_ = json.value("loop", true);
    playbackSpeed_ = json.value("playbackSpeed", 1.0f);
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
