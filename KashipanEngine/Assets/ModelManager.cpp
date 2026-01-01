#include "ModelManager.h"

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

using Handle = ModelManager::ModelHandle;

struct ModelEntry final {
    std::string fullPath;
    std::string assetPath;
    std::string fileName;

    ModelData data;
};

std::unordered_map<Handle, ModelEntry> sModels;
std::unordered_map<std::string, Handle> sFileNameToHandle;
std::unordered_map<std::string, Handle> sAssetPathToHandle;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool HasSupportedModelExtension(const std::filesystem::path& p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".3ds" || ext == ".blend" || ext == ".ply" || ext == ".stl" || ext == ".x");
}

std::string MakeAssetRelativePath(const std::string& assetsRoot, const std::string& fullPath) {
    std::filesystem::path root(assetsRoot);
    std::filesystem::path full(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(full.filename().string());
    }
    return NormalizePathSlashes(rel.string());
}

Handle RegisterEntry(ModelEntry&& entry) {
    // Handle 0 is invalid. Use compact increasing handle.
    const Handle handle = static_cast<Handle>(sModels.size() + 1u);
    if (handle == ModelManager::kInvalidHandle) return ModelManager::kInvalidHandle;
    if (sModels.find(handle) != sModels.end()) return ModelManager::kInvalidHandle;

    sFileNameToHandle[entry.fileName] = handle;
    sAssetPathToHandle[NormalizePathSlashes(entry.assetPath)] = handle;
    sModels.emplace(handle, std::move(entry));
    return handle;
}

} // namespace

ModelManager::ModelManager(Passkey<GameEngine>, const std::string& assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    LoadAllFromAssetsFolder();
}

ModelManager::~ModelManager() {
    LogScope scope;
    sModels.clear();
    sFileNameToHandle.clear();
    sAssetPathToHandle.clear();
}

void ModelManager::LoadAllFromAssetsFolder() {
    LogScope scope;
    const auto dir = GetDirectoryData(assetsRootPath_, true, true);

    std::vector<std::string> files;
    const auto filtered = GetDirectoryDataByExtension(dir,
        { ".fbx", ".obj", ".gltf", ".glb", ".dae", ".3ds", ".blend", ".ply", ".stl", ".x" });

    std::function<void(const DirectoryData&)> flatten = [&](const DirectoryData& d) {
        for (const auto& f : d.files) files.push_back(f);
        for (const auto& sd : d.subdirectories) flatten(sd);
    };
    flatten(filtered);

    for (const auto& f : files) {
        LoadModel(f);
    }
}

ModelManager::ModelHandle ModelManager::LoadModel(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    Log(Translation("engine.model.loading.start") + filePath, LogSeverity::Info);

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.model.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return it->second;
        }
    }

    std::filesystem::path p(filePath);

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.model.loading.failed.notfound") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }
    if (!HasSupportedModelExtension(p)) {
        Log(Translation("engine.model.loading.failed.unsupported") + p.string(), LogSeverity::Warning);
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

    const aiScene* scene = importer.ReadFile(p.string(), flags);
    if (!scene || !scene->mRootNode) {
        Log(Translation("engine.model.loading.failed.assimp") + p.string() + " msg=" + importer.GetErrorString(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    ModelEntry entry{};
    entry.fullPath = NormalizePathSlashes(p.string());
    entry.assetPath = MakeAssetRelativePath(assetsRootPath_, entry.fullPath);
    entry.fileName = p.filename().string();

    // Set asset relative path in ModelData
    entry.data.assetRelativePath_ = entry.assetPath;

    // Extract materials from the scene and store minimal material data
    for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
        const aiMaterial* mat = scene->mMaterials[mi];
        if (!mat) continue;
        ModelData::MaterialData md;
        // base/diffuse color
        aiColor4D diffColor(1.0f, 1.0f, 1.0f, 1.0f);
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, diffColor)) {
            md.baseColor[0] = diffColor.r;
            md.baseColor[1] = diffColor.g;
            md.baseColor[2] = diffColor.b;
            md.baseColor[3] = diffColor.a;
        }
        // diffuse texture (first diffuse slot)
        aiString texPath;
        if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            std::string tp = texPath.C_Str();
            if (!tp.empty() && tp[0] != '*') {
                // Resolve relative to model file directory
                std::filesystem::path texFull = std::filesystem::path(p).parent_path() / tp;
                md.diffuseTexturePath = MakeAssetRelativePath(assetsRootPath_, NormalizePathSlashes(texFull.string()));
            }
        }
        entry.data.materials_.push_back(std::move(md));
    }

    // ModelData は private メンバを持つため、friend である ModelManager（この関数内）でのみ構築する
    const auto appendMesh = [](const aiMesh* mesh, ModelData& dst) {
        if (!mesh) return;

        const bool hasNormals = mesh->HasNormals();
        const bool hasUV0 = mesh->HasTextureCoords(0);

        const uint32_t baseVertex = static_cast<uint32_t>(dst.vertices_.size());

        dst.vertices_.reserve(dst.vertices_.size() + static_cast<size_t>(mesh->mNumVertices));

        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            ModelData::Vertex v{};
            v.px = mesh->mVertices[i].x;
            v.py = mesh->mVertices[i].y;
            v.pz = mesh->mVertices[i].z;

            if (hasNormals) {
                v.nx = mesh->mNormals[i].x;
                v.ny = mesh->mNormals[i].y;
                v.nz = mesh->mNormals[i].z;
            }

            if (hasUV0) {
                v.u = mesh->mTextureCoords[0][i].x;
                v.v = mesh->mTextureCoords[0][i].y;
            }

            dst.vertices_.push_back(v);
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                dst.indices_.push_back(baseVertex + face.mIndices[j]);
            }
        }
    };

    std::function<void(const aiNode*)> appendNodeMeshes;
    appendNodeMeshes = [&](const aiNode* node) {
        if (!node) return;
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const unsigned int meshIdx = node->mMeshes[i];
            if (meshIdx >= scene->mNumMeshes) continue;
            appendMesh(scene->mMeshes[meshIdx], entry.data);
        }
        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            appendNodeMeshes(node->mChildren[c]);
        }
    };

    appendNodeMeshes(scene->mRootNode);

    if (entry.data.GetVertexCount() == 0 || entry.data.GetIndexCount() == 0) {
        Log(Translation("engine.model.loading.failed.nomesh") + p.string(), LogSeverity::Warning);
        return kInvalidHandle;
    }

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.model.loading.failed.register") + p.string(), LogSeverity::Error);
        return kInvalidHandle;
    }

    Log(Translation("engine.model.loading.succeeded") + p.string(), LogSeverity::Info);
    return handle;
}

ModelManager::ModelHandle ModelManager::GetModelHandleFromFileName(const std::string& fileName) {
    LogScope scope;
    auto it = sFileNameToHandle.find(fileName);
    if (it == sFileNameToHandle.end()) return kInvalidHandle;
    return it->second;
}

ModelManager::ModelHandle ModelManager::GetModelHandleFromAssetPath(const std::string& assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) return kInvalidHandle;
    return it->second;
}

const ModelData &ModelManager::GetModelData(ModelHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) return sEmptyData;
    auto it = sModels.find(handle);
    if (it == sModels.end()) return sEmptyData;
    return it->second.data;
}

const ModelData &ModelManager::GetModelDataFromFileName(const std::string &fileName) {
    LogScope scope;
    const auto h = GetModelHandleFromFileName(fileName);
    if (h == kInvalidHandle) return sEmptyData;
    return GetModelData(h);
}

const ModelData &ModelManager::GetModelDataFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    const auto h = GetModelHandleFromAssetPath(assetPath);
    if (h == kInvalidHandle) return sEmptyData;
    return GetModelData(h);
}

#if defined(USE_IMGUI)
std::vector<ModelManager::ModelHandle> ModelManager::GetAllImGuiModels() {
    LogScope scope;
    std::vector<ModelHandle> out;
    out.reserve(sModels.size());
    for (const auto& kv : sModels) out.push_back(kv.first);
    return out;
}

std::vector<ModelManager::ModelListEntry> ModelManager::GetImGuiModelListEntries() {
    LogScope scope;
    std::vector<ModelListEntry> out;
    out.reserve(sModels.size());

    for (const auto& kv : sModels) {
        const auto& m = kv.second;
        ModelListEntry e;
        e.handle = kv.first;
        e.fileName = m.fileName;
        e.assetPath = m.assetPath;
        e.vertexCount = m.data.GetVertexCount();
        e.indexCount = m.data.GetIndexCount();
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const ModelListEntry& a, const ModelListEntry& b) {
        return a.assetPath < b.assetPath;
    });

    return out;
}
#endif

} // namespace KashipanEngine
