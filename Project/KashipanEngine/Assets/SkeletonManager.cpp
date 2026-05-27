#include "SkeletonManager.h"
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

using Handle = SkeletonManager::SkeletonHandle;

struct SkeletonEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    SkeletonData data;
};

std::unordered_map<Handle, SkeletonEntry> sSkeletons;
FileMap<Handle> sFileNameToHandle;
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

bool HasSupportedSkeletonExtension(const std::filesystem::path &p) {
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

Handle RegisterEntry(SkeletonEntry &&entry) {
    const Handle handle = static_cast<Handle>(sSkeletons.size() + 1u);
    if (handle == SkeletonManager::kInvalidHandle) return SkeletonManager::kInvalidHandle;
    if (sSkeletons.find(handle) != sSkeletons.end()) return SkeletonManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sSkeletons.emplace(handle, std::move(entry));
    return handle;
}

Matrix4x4 ConvertMatrix(const aiMatrix4x4 &m) {
    Matrix4x4 out{};
    out.m[0][0] = m.a1;
    out.m[0][1] = m.a2;
    out.m[0][2] = m.a3;
    out.m[0][3] = m.a4;
    out.m[1][0] = m.b1;
    out.m[1][1] = m.b2;
    out.m[1][2] = m.b3;
    out.m[1][3] = m.b4;
    out.m[2][0] = m.c1;
    out.m[2][1] = m.c2;
    out.m[2][2] = m.c3;
    out.m[2][3] = m.c4;
    out.m[3][0] = m.d1;
    out.m[3][1] = m.d2;
    out.m[3][2] = m.d3;
    out.m[3][3] = m.d4;
    return out;
}

std::unique_ptr<Transform3D> ConvertTransform(const aiMatrix4x4 &m) {
    auto transform = std::make_unique<Transform3D>();

    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D translation;
    m.Decompose(scaling, rotation, translation);

    transform->SetScale(Vector3(scaling.x, scaling.y, scaling.z));
    transform->SetRotateQuaternion(Quaternion(rotation.x, rotation.y, rotation.z, rotation.w));
    transform->SetTranslate(Vector3(translation.x, translation.y, translation.z));
    return std::move(transform);
}

Node BuildNode(const aiNode *node) {
    Node out{};
    if (!node) return out;
    out.name = node->mName.C_Str();
    out.transform = ConvertTransform(node->mTransformation);
    out.children.reserve(node->mNumChildren);
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        out.children.push_back(BuildNode(node->mChildren[i]));
    }
    return out;
}

void CollectBones(const aiScene *scene, Skeleton &skeleton) {
    if (!scene) return;

    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh *mesh = scene->mMeshes[mi];
        if (!mesh) continue;

        for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi) {
            const aiBone *bone = mesh->mBones[bi];
            if (!bone) continue;

            const std::string boneName = bone->mName.C_Str();
            if (boneName.empty()) continue;

            if (skeleton.jointNameToIndexMap.find(boneName) != skeleton.jointNameToIndexMap.end()) {
                continue;
            }

            SkeletonJoint joint{};
            joint.name = boneName;
            skeleton.jointNameToIndexMap[boneName] = static_cast<int32_t>(skeleton.joints.size());
            skeleton.joints.push_back(std::move(joint));
        }
    }
}

void BuildJointHierarchy(const aiNode *node, Skeleton &skeleton) {
    if (!node) return;

    const std::string nodeName = node->mName.C_Str();
    const auto it = skeleton.jointNameToIndexMap.find(nodeName);
    const int32_t jointIndex = (it != skeleton.jointNameToIndexMap.end()) ? it->second : -1;

    if (jointIndex >= 0) {
        SkeletonJoint &joint = skeleton.joints[static_cast<size_t>(jointIndex)];
        joint.transform = ConvertTransform(node->mTransformation);
        joint.skeletonSpaceTransform = std::make_unique<Transform3D>();
        joint.skeletonSpaceTransform->SetParentTransform(joint.transform.get());

        if (node->mParent) {
            const std::string parentName = node->mParent->mName.C_Str();
            const auto parentIt = skeleton.jointNameToIndexMap.find(parentName);
            if (parentIt != skeleton.jointNameToIndexMap.end()) {
                joint.parentIndex = parentIt->second;
                skeleton.joints[static_cast<size_t>(parentIt->second)].childrenIndices.push_back(jointIndex);
                auto *parentTransform = skeleton.joints[static_cast<size_t>(parentIt->second)].transform.get();
                joint.transform->SetParentTransform(parentTransform);
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        BuildJointHierarchy(node->mChildren[i], skeleton);
    }
}

} // namespace

SkeletonManager::SkeletonManager(Passkey<GameEngine>, const std::string &assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    LoadAllFromAssetsFolder();
}

SkeletonManager::~SkeletonManager() {
    LogScope scope;
    sSkeletons.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
}

void SkeletonManager::LoadAllFromAssetsFolder() {
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
        LoadSkeleton(f);
    }
}

SkeletonManager::SkeletonHandle SkeletonManager::LoadSkeleton(const std::string &filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    Log(Translation("engine.skeleton.loading.start") + filePath, LogSeverity::Info);

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.skeleton.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return it->second;
        }
    }

    std::filesystem::path p(filePath);

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.skeleton.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedSkeletonExtension(p)) {
        Log(Translation("engine.skeleton.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
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
        Log(Translation("engine.skeleton.loading.failed.assimp") + p.string() + " msg=" + importer.GetErrorString(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    SkeletonEntry entry{};
    entry.fullPath = NormalizePathSlashes(p.string());
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = p.filename().string();
    entry.data.assetRelativePath_ = entry.assetPath;

    entry.data.rootNode_ = BuildNode(scene->mRootNode);

    Skeleton skeleton{};
    skeleton.rootJointIndex = -1;
    CollectBones(scene, skeleton);
    if (skeleton.joints.empty()) {
        Log(Translation("engine.skeleton.loading.failed.noskeleton") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }
    BuildJointHierarchy(scene->mRootNode, skeleton);

    if (!skeleton.joints.empty()) {
        for (size_t i = 0; i < skeleton.joints.size(); ++i) {
            if (!skeleton.joints[i].parentIndex.has_value()) {
                skeleton.rootJointIndex = static_cast<int32_t>(i);
                break;
            }
        }
    }

    entry.data.skeleton_ = std::move(skeleton);

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.skeleton.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.skeleton.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

SkeletonManager::SkeletonHandle SkeletonManager::GetSkeletonHandleFromFileName(const std::string &fileName) {
    if (fileName.empty()) return kInvalidHandle;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

SkeletonManager::SkeletonHandle SkeletonManager::GetSkeletonHandleFromAssetPath(const std::string &assetPath) {
    if (assetPath.empty()) return kInvalidHandle;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

const SkeletonData &SkeletonManager::GetSkeletonData(SkeletonHandle handle) {
    if (handle == kInvalidHandle) return sEmptyData;
    auto it = sSkeletons.find(handle);
    if (it == sSkeletons.end()) return sEmptyData;
    return it->second.data;
}

const SkeletonData &SkeletonManager::GetSkeletonDataFromFileName(const std::string &fileName) {
    return GetSkeletonData(GetSkeletonHandleFromFileName(fileName));
}

const SkeletonData &SkeletonManager::GetSkeletonDataFromAssetPath(const std::string &assetPath) {
    return GetSkeletonData(GetSkeletonHandleFromAssetPath(assetPath));
}

std::vector<SkeletonManager::SkeletonListEntry> SkeletonManager::GetLoadedSkeletonListEntries() {
    std::vector<SkeletonListEntry> list;
    list.reserve(sSkeletons.size());
    for (const auto &pair : sSkeletons) {
        SkeletonListEntry entry{};
        entry.handle = pair.first;
        entry.fileName = pair.second.fileName;
        entry.assetPath = pair.second.assetPath;
        entry.jointCount = static_cast<uint32_t>(pair.second.data.GetSkeleton().joints.size());
        list.push_back(std::move(entry));
    }
    return list;
}

const bool SkeletonManager::UpdateSkeletonJointTransforms(SkeletonHandle handle) {
    auto it = sSkeletons.find(handle);
    if (it == sSkeletons.end()) {
        return false;
    }

    SkeletonData &data = it->second.data;
    const Skeleton &skeleton = data.GetSkeleton();
    for (const auto &joint : skeleton.joints) {
        if (auto *t = joint.skeletonSpaceTransform.get()) {
            // ワールド行列取得時の行列更新処理を利用して、全スケルトンの行列を更新する
            t->GetWorldMatrix();
        }
    }
    return true;
}

} // namespace KashipanEngine
