#include "AnimationManager.h"
#include "Assets/CaseInsensitive.h"

#include "Debug/Logger.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Translation.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <utility>

namespace KashipanEngine {

namespace {

using Handle = AnimationManager::AnimationHandle;

struct AnimationEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    AnimationData data;
};

std::unordered_map<Handle, AnimationEntry> sAnimations;
// 異なるフォルダに同名ファイルが存在する場合があるため、ファイル名1つに対して複数のハンドルを保持する
FileMap<std::vector<Handle>> sFileNameToHandle;
FileMap<Handle> sAssetPathToHandle;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedAnimationExtension(const std::filesystem::path &p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".x" || ext == ".blend" || ext == ".obj");
}

std::string MakeAssetRelativePath(const std::string &assetsRoot, const std::string &fullPath) {
    std::filesystem::path root(assetsRoot);
    std::filesystem::path full(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(full.filename().string());
    }
    return NormalizePathSlashes(rel.string());
}

Handle RegisterEntry(AnimationEntry &&entry) {
    const Handle handle = static_cast<Handle>(sAnimations.size() + 1u);
    if (handle == AnimationManager::kInvalidHandle) return AnimationManager::kInvalidHandle;
    if (sAnimations.find(handle) != sAnimations.end()) return AnimationManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName].push_back(handle);
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sAnimations.emplace(handle, std::move(entry));
    return handle;
}

KeyframeTimeline BuildTimeline(const std::string &name, KeyframeValueType valueType, const std::vector<KeyframeNode> &keys, bool loop) {
    KeyframeTimeline timeline;
    timeline.name = name;
    timeline.valueType = valueType;
    timeline.keys = keys;
    timeline.loop = loop;
    if (!timeline.keys.empty()) {
        float maxTime = timeline.keys.front().time;
        for (const auto &k : timeline.keys) {
            if (k.time > maxTime) maxTime = k.time;
        }
        timeline.duration = maxTime;
    }
    return timeline;
}

} // namespace

AnimationManager::AnimationManager(Passkey<GameEngine>, const std::string &assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    LoadAllFromAssetsFolder();
}

AnimationManager::~AnimationManager() {
    LogScope scope;
    sAnimations.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
}

void AnimationManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir,
        { ".fbx", ".gltf", ".glb", ".dae", ".x", ".blend", ".obj" });

    std::function<void(const DirectoryData &)> flatten = [&](const DirectoryData &d) {
        for (const auto &f : d.files) files.push_back(f);
        for (const auto &sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto &f : files) {
        LoadAnimation(f);
    }
}

AnimationManager::AnimationHandle AnimationManager::LoadAnimation(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    Log(Translation("engine.animation.loading.start") + filePath, LogSeverity::Info);

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.animation.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return it->second;
        }
    }

    std::filesystem::path p(filePath);

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.animation.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedAnimationExtension(p)) {
        Log(Translation("engine.animation.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    Assimp::Importer importer;
    constexpr unsigned int flags =
        aiProcess_MakeLeftHanded |
        aiProcess_FlipWindingOrder |
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_ImproveCacheLocality |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_FindInvalidData |
        aiProcess_TransformUVCoords |
        aiProcess_SortByPType |
        aiProcess_FlipUVs;

    const aiScene *scene = importer.ReadFile(p.string(), flags);
    if (!scene || !scene->mRootNode) {
        Log(Translation("engine.animation.loading.failed.assimp") + p.string() + " msg=" + importer.GetErrorString(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    if (scene->mNumAnimations == 0) {
        Log(Translation("engine.animation.loading.failed.noanim") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    AnimationEntry entry{};
    entry.fullPath = NormalizePathSlashes(p.string());
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = p.filename().string();
    entry.data.assetRelativePath_ = entry.assetPath;

    entry.data.clips_.reserve(scene->mNumAnimations);

    for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai) {
        const aiAnimation *anim = scene->mAnimations[ai];
        if (!anim) continue;

        AnimationClip clip;
        clip.name = anim->mName.length > 0 ? anim->mName.C_Str() : (entry.fileName + "#" + std::to_string(ai));
        clip.duration = static_cast<float>(anim->mDuration);
        clip.ticksPerSecond = static_cast<float>(anim->mTicksPerSecond);
        if (clip.ticksPerSecond <= 0.0f) {
            clip.ticksPerSecond = 30.0f;
        }

        const float ticksPerSecond = clip.ticksPerSecond;
        const float maxTimeSec = (clip.duration > 0.0f && ticksPerSecond > 0.0f) ? (clip.duration / ticksPerSecond) : 0.0f;

        for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci) {
            const aiNodeAnim *channel = anim->mChannels[ci];
            if (!channel) continue;
            const std::string nodeName = channel->mNodeName.C_Str();

            std::vector<KeyframeNode> xKeys;
            std::vector<KeyframeNode> yKeys;
            std::vector<KeyframeNode> zKeys;

            xKeys.reserve(channel->mNumPositionKeys + channel->mNumScalingKeys + channel->mNumRotationKeys);
            yKeys.reserve(channel->mNumPositionKeys + channel->mNumScalingKeys + channel->mNumRotationKeys);
            zKeys.reserve(channel->mNumPositionKeys + channel->mNumScalingKeys + channel->mNumRotationKeys);

            for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
                const auto &pk = channel->mPositionKeys[k];
                const float t = static_cast<float>(pk.mTime / ticksPerSecond);
                xKeys.push_back({ t, static_cast<float>(pk.mValue.x), EaseType::Linear });
                yKeys.push_back({ t, static_cast<float>(pk.mValue.y), EaseType::Linear });
                zKeys.push_back({ t, static_cast<float>(pk.mValue.z), EaseType::Linear });
            }

            if (!xKeys.empty()) {
                clip.timelines.push_back(BuildTimeline(nodeName + ".Translate.X", KeyframeValueType::Float, xKeys, false));
                clip.timelineNameToIndex[nodeName + ".Translate.X"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Translate.X"]);
                clip.timelines.push_back(BuildTimeline(nodeName + ".Translate.Y", KeyframeValueType::Float, yKeys, false));
                clip.timelineNameToIndex[nodeName + ".Translate.Y"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Translate.Y"]);
                clip.timelines.push_back(BuildTimeline(nodeName + ".Translate.Z", KeyframeValueType::Float, zKeys, false));
                clip.timelineNameToIndex[nodeName + ".Translate.Z"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Translate.Z"]);
            }

            xKeys.clear();
            yKeys.clear();
            zKeys.clear();

            for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
                const auto &sk = channel->mScalingKeys[k];
                const float t = static_cast<float>(sk.mTime / ticksPerSecond);
                xKeys.push_back({ t, static_cast<float>(sk.mValue.x), EaseType::Linear });
                yKeys.push_back({ t, static_cast<float>(sk.mValue.y), EaseType::Linear });
                zKeys.push_back({ t, static_cast<float>(sk.mValue.z), EaseType::Linear });
            }

            if (!xKeys.empty()) {
                clip.timelines.push_back(BuildTimeline(nodeName + ".Scale.X", KeyframeValueType::Float, xKeys, false));
                clip.timelineNameToIndex[nodeName + ".Scale.X"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Scale.X"]);
                clip.timelines.push_back(BuildTimeline(nodeName + ".Scale.Y", KeyframeValueType::Float, yKeys, false));
                clip.timelineNameToIndex[nodeName + ".Scale.Y"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Scale.Y"]);
                clip.timelines.push_back(BuildTimeline(nodeName + ".Scale.Z", KeyframeValueType::Float, zKeys, false));
                clip.timelineNameToIndex[nodeName + ".Scale.Z"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Scale.Z"]);
            }

            xKeys.clear();
            yKeys.clear();
            zKeys.clear();

            std::vector<KeyframeNode> qKeys;
            qKeys.reserve(channel->mNumRotationKeys);

            for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
                const auto &rk = channel->mRotationKeys[k];
                const float t = static_cast<float>(rk.mTime / ticksPerSecond);
                const auto q = rk.mValue;
                qKeys.push_back({ t, Quaternion(static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z), static_cast<float>(q.w)), EaseType::Linear });
            }

            if (!qKeys.empty()) {
                clip.timelines.push_back(BuildTimeline(nodeName + ".Rotate", KeyframeValueType::Quaternion, qKeys, false));
                clip.timelineNameToIndex[nodeName + ".Rotate"] = static_cast<uint32_t>(clip.timelines.size() - 1);
                clip.nodeNameToTimelineIndices[nodeName].push_back(clip.timelineNameToIndex[nodeName + ".Rotate"]);
            }
        }

        if (clip.timelines.empty()) {
            KeyframeTimeline t;
            t.name = clip.name + ".Empty";
            t.duration = maxTimeSec;
            t.loop = false;
            clip.timelines.push_back(t);
        }

        entry.data.clipNameToIndex_[clip.name] = static_cast<uint32_t>(entry.data.clips_.size());
        entry.data.clips_.push_back(std::move(clip));
    }

    if (entry.data.clips_.empty()) {
        Log(Translation("engine.animation.loading.failed.noanim") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.animation.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.animation.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

AnimationManager::AnimationHandle AnimationManager::GetAnimationHandleFromFileName(const std::string &fileName) {
    const auto &handles = GetAnimationHandlesFromFileName(fileName);
    return handles.empty() ? kInvalidHandle : handles.front();
}

const std::vector<AnimationManager::AnimationHandle> &AnimationManager::GetAnimationHandlesFromFileName(const std::string &fileName) {
    if (fileName.empty()) return sEmptyHandleList;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return sEmptyHandleList;
    return it->second;
}

AnimationManager::AnimationHandle AnimationManager::GetAnimationHandleFromAssetPath(const std::string &assetPath) {
    if (assetPath.empty()) return kInvalidHandle;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

const AnimationData &AnimationManager::GetAnimationData(AnimationHandle handle) {
    if (handle == kInvalidHandle) return sEmptyData;
    auto it = sAnimations.find(handle);
    if (it == sAnimations.end()) return sEmptyData;
    return it->second.data;
}

const AnimationData &AnimationManager::GetAnimationDataFromFileName(const std::string &fileName) {
    return GetAnimationData(GetAnimationHandleFromFileName(fileName));
}

const AnimationData &AnimationManager::GetAnimationDataFromAssetPath(const std::string &assetPath) {
    return GetAnimationData(GetAnimationHandleFromAssetPath(assetPath));
}

std::vector<AnimationManager::AnimationListEntry> AnimationManager::GetLoadedAnimationListEntries() {
    std::vector<AnimationListEntry> list;
    list.reserve(sAnimations.size());
    for (const auto &[handle, entry] : sAnimations) {
        AnimationListEntry e;
        e.handle = handle;
        e.fileName = entry.fileName;
        e.assetPath = entry.assetPath;
        e.clipCount = static_cast<uint32_t>(entry.data.GetClipCount());
        list.push_back(std::move(e));
    }
    return list;
}

} // namespace KashipanEngine
