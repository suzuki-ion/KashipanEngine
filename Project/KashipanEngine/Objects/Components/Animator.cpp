#include "Animator.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "Debug/Logger.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Transform.h"
#include "Scene/SceneContext.h"
#include "Utilities/TimeUtils.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/Translation.h"
#endif

namespace KashipanEngine {

namespace {

float SampleFloatTimeline(const KeyframeTimeline &timeline, float time, bool loop) {
    LogScope scope;
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
    LogScope scope;
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
void ApplyClipToSkeletonJoints(const AnimationClip &clip, const Skeleton &skeleton, float time, bool loop) {
    LogScope scope;
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

void Animator::Initialize() {
    LogScope scope;
    isPlaying_ = playOnStart_;
    RebuildSkeletonInstanceIfNeeded();
}

void Animator::Update() {
    LogScope scope;
    RebuildSkeletonInstanceIfNeeded();
    if (skeletonInstance_.joints.empty()) return;
    AdvanceAnimation(GetDeltaTime() * GetGameSpeed());

    // アニメーション再生中は、シーン上のアーマチュア用オブジェクトの位置をジョイント姿勢と紐づけする
    if (isPlaying_ && !clipName_.empty()) {
        SyncArmatureObjects();
    }
}

ModelManager::ModelHandle Animator::GetMeshHandle() const {
    LogScope scope;
    auto *objectContext = GetOwnerObjectContext();
    if (!objectContext) return ModelManager::kInvalidHandle;
    auto *meshFilter = objectContext->GetComponent<MeshFilter>();
    if (!meshFilter) return ModelManager::kInvalidHandle;
    return meshFilter->GetMeshHandle();
}

void Animator::RebuildSkeletonInstanceIfNeeded() {
    LogScope scope;
    const auto meshHandle = GetMeshHandle();
    if (meshHandle == ModelManager::kInvalidHandle) return;

    const bool meshChanged = meshHandle != lastMeshHandle_;
    if (meshChanged) {
        lastMeshHandle_ = meshHandle;
        const auto &modelData = ModelManager::GetModelData(meshHandle);
        // サブメッシュ（"モデルパス:ノード名"）の場合もスケルトン・アニメーションはモデルファイル単位で
        // 管理されているため、元のモデルファイルのアセットパスで解決する
        baseAssetPath_ = ModelManager::GetBaseAssetPath(modelData.GetAssetRelativePath());
        skeletonHandle_ = SkeletonManager::GetSkeletonHandleFromAssetPath(baseAssetPath_);
        // SkeletonManagerが保持する共有アセット本体ではなく、このコンポーネント専用に複製した
        // スケルトンを使う（同じスケルトンアセットを参照する複数のAnimatorが互いのアニメーション
        // 再生状態に干渉しないようにするため）
        skeletonInstance_ = SkeletonManager::CloneSkeleton(skeletonHandle_);
        elapsedTime_ = 0.0f;
    }

    if (meshChanged || animationSourceAssetPath_ != lastAnimationSourceAssetPath_) {
        lastAnimationSourceAssetPath_ = animationSourceAssetPath_;
        // animationSourceAssetPath_が空の場合はメッシュ自身のファイルから、指定されている場合は
        // そのファイルからアニメーションを取得する（同じボーン名を持つ別ファイルの共有に対応するため）
        const std::string &animAssetPath = animationSourceAssetPath_.empty() ? baseAssetPath_ : animationSourceAssetPath_;
        animationHandle_ = AnimationManager::GetAnimationHandleFromAssetPath(animAssetPath);
    }
}

void Animator::AdvanceAnimation(float deltaTime) {
    LogScope scope;
    if (!isPlaying_ || clipName_.empty()) return;
    if (skeletonHandle_ == SkeletonManager::kInvalidHandle || animationHandle_ == AnimationManager::kInvalidHandle) return;

    const auto &animData = AnimationManager::GetAnimationData(animationHandle_);
    const AnimationClip *clip = animData.FindClipByName(clipName_);
    if (!clip) return;

    elapsedTime_ += deltaTime * playbackSpeed_;
    ApplyClipToSkeletonJoints(*clip, skeletonInstance_, elapsedTime_, loop_);

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

void Animator::SyncArmatureObjects() {
    LogScope scope;
    if (skeletonInstance_.joints.empty()) return;
    auto *sceneContext = GetOwnerSceneContext();
    if (!sceneContext) return;

    // 検索の起点: Root Boneが設定されていればそのオブジェクト、無ければ所有オブジェクトの最上位の祖先
    // （モデルのプレハブではアーマチュアとメッシュオブジェクトが同じルートの子孫になっているため）
    EmptyObject *searchRoot = GetRootBoneObject();
    if (!searchRoot) {
        auto *objectContext = GetOwnerObjectContext();
        auto *owner = objectContext ? const_cast<EmptyObject *>(objectContext->GetOwner()) : nullptr;
        if (!owner) return;
        searchRoot = owner;
        while (true) {
            auto *transform = searchRoot->GetComponent<Transform>();
            EmptyObject *parent = transform ? transform->GetParentObject() : nullptr;
            if (!parent) break;
            searchRoot = parent;
        }
    }

    // 起点の子孫（起点自身を含む）を名前で引けるようにする
    std::unordered_map<std::string, EmptyObject *> nameToObject;
    for (auto *obj : sceneContext->GetSceneObjects()) {
        if (!obj) continue;
        EmptyObject *cursor = obj;
        bool isInSubtree = false;
        while (cursor) {
            if (cursor == searchRoot) {
                isInSubtree = true;
                break;
            }
            auto *transform = cursor->GetComponent<Transform>();
            cursor = transform ? transform->GetParentObject() : nullptr;
        }
        if (!isInSubtree) continue;
        nameToObject.emplace(obj->GetName(), obj);
    }
    if (nameToObject.empty()) return;

    // ジョイントのローカルTRSを同名オブジェクトのTransformへ書き込む
    // （プレハブのアーマチュア階層はモデルのノード階層と同じ構造のため、ローカル値が1:1で対応する）
    for (const auto &joint : skeletonInstance_.joints) {
        if (!joint.transform) continue;
        auto it = nameToObject.find(joint.name);
        if (it == nameToObject.end()) continue;
        auto *transform = it->second->GetComponent<Transform>();
        if (!transform) continue;
        transform->SetTranslate(joint.transform->GetTranslate());
        transform->SetRotateQuaternion(joint.transform->GetRotate());
        transform->SetScale(joint.transform->GetScale());
    }
}

void Animator::ResetToBindPose() {
    LogScope scope;
    for (auto &joint : skeletonInstance_.joints) {
        if (auto *t = joint.transform.get()) t->ResetToBindPose();
        if (auto *t = joint.skeletonSpaceTransform.get()) t->ResetToBindPose();
    }
    elapsedTime_ = 0.0f;
}

std::vector<std::string> Animator::GetAvailableClipNames() const {
    std::vector<std::string> names;
    const auto meshHandle = GetMeshHandle();
    if (meshHandle == ModelManager::kInvalidHandle) return names;
    const auto &modelData = ModelManager::GetModelData(meshHandle);
    const std::string baseAssetPath = ModelManager::GetBaseAssetPath(modelData.GetAssetRelativePath());
    const std::string &animAssetPath = animationSourceAssetPath_.empty() ? baseAssetPath : animationSourceAssetPath_;
    const auto animHandle = AnimationManager::GetAnimationHandleFromAssetPath(animAssetPath);
    if (animHandle == AnimationManager::kInvalidHandle) return names;
    const auto &animData = AnimationManager::GetAnimationData(animHandle);
    for (const auto &clip : animData.GetClips()) {
        names.push_back(clip.name);
    }
    return names;
}

#if defined(USE_IMGUI)
void Animator::ShowImGui() {
    TargetObjectSelector::ShowSelector(TranslationLabel("component.animator.root_bone"), GetOwnerSceneContext(), rootBoneObjectID_);

    const auto clipNames = GetAvailableClipNames();
    if (ImGuiCustom::SelectString(TranslationLabel("component.animator.animation_clip"), clipName_, clipNames, true)) {
        elapsedTime_ = 0.0f;
    }

    // アニメーション取得元（未指定の場合はメッシュ自身のファイルから取得する）
    std::vector<std::string> animAssetPaths;
    for (const auto &entry : AnimationManager::GetLoadedAnimationListEntries()) {
        animAssetPaths.push_back(entry.assetPath);
    }
    std::string sourcePath = animationSourceAssetPath_;
    if (ImGuiCustom::SelectString(TranslationLabel("component.animator.animation_source"), sourcePath, animAssetPaths, true)) {
        animationSourceAssetPath_ = sourcePath;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(TranslationC("component.animator.desc_1"));
    }

    ImGui::Checkbox(TranslationLabel("component.animator.play_on_start"), &playOnStart_);
    ImGui::Checkbox(TranslationLabel("component.animator.loop"), &loop_);
    ImGui::DragFloat(TranslationLabel("component.animator.playback_speed"), &playbackSpeed_, 0.01f, 0.0f, 10.0f);
    if (ImGui::Button(isPlaying_ ? TranslationLabel("component.animator.stop") : TranslationLabel("component.animator.play"))) {
        isPlaying_ = !isPlaying_;
    }
    ImGui::Text(TranslationC("component.animator.joint_count_d"), static_cast<int>(skeletonInstance_.joints.size()));
}
#endif

JSON Animator::SaveToJson() const {
    JSON json = JSON::object();
    json["rootBoneObjectID"] = ToJSON(rootBoneObjectID_);
    json["clipName"] = clipName_;
    json["animationSourceAssetPath"] = animationSourceAssetPath_;
    json["playOnStart"] = playOnStart_;
    json["loop"] = loop_;
    json["playbackSpeed"] = playbackSpeed_;
    return json;
}

bool Animator::LoadFromJson(const JSON &json) {
    if (json.contains("rootBoneObjectID")) {
        rootBoneObjectID_ = FromJSON<UUID128>(json["rootBoneObjectID"]);
    } else {
        rootBoneObjectID_ = UUID128();
    }
    clipName_ = json.value("clipName", std::string{});
    animationSourceAssetPath_ = json.value("animationSourceAssetPath", std::string{});
    playOnStart_ = json.value("playOnStart", true);
    loop_ = json.value("loop", true);
    playbackSpeed_ = json.value("playbackSpeed", 1.0f);
    return true;
}

} // namespace KashipanEngine
