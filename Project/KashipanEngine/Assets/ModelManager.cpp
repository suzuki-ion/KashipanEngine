#include "ModelManager.h"
#include "Assets/CaseInsensitive.h"

#include "Debug/Logger.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/Translation.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_internal.h>
#endif

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
FileMap<Handle> sFileNameToHandle;
FileMap<Handle> sAssetPathToHandle;

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

void AddBoneInfluence(ModelData::Vertex &vertex, uint32_t boneIndex, float weight) {
    if (weight <= 0.0f) return;

    for (size_t i = 0; i < 4; ++i) {
        if (vertex.boneWeights[i] == 0.0f) {
            vertex.boneIndices[i] = boneIndex;
            vertex.boneWeights[i] = weight;
            return;
        }
    }

    size_t minIndex = 0;
    for (size_t i = 1; i < 4; ++i) {
        if (vertex.boneWeights[i] < vertex.boneWeights[minIndex]) {
            minIndex = i;
        }
    }
    if (weight > vertex.boneWeights[minIndex]) {
        vertex.boneIndices[minIndex] = boneIndex;
        vertex.boneWeights[minIndex] = weight;
    }
}

void NormalizeBoneWeights(ModelData::Vertex &vertex) {
    float total = 0.0f;
    for (float w : vertex.boneWeights) {
        total += w;
    }
    if (total <= 0.0f) return;
    for (float &w : vertex.boneWeights) {
        w /= total;
    }
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

        for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi) {
            const aiBone *bone = mesh->mBones[bi];
            if (!bone) continue;

            const std::string boneName = bone->mName.C_Str();
            if (boneName.empty()) continue;

            uint32_t boneIndex = 0;
            auto clusterIt = dst.skinClusters_.find(boneName);
            if (clusterIt == dst.skinClusters_.end()) {
                boneIndex = static_cast<uint32_t>(dst.skinClusterNames_.size());
                dst.skinClusterNames_.push_back(boneName);
            } else {
                const auto nameIt = std::find(dst.skinClusterNames_.begin(), dst.skinClusterNames_.end(), boneName);
                boneIndex = nameIt != dst.skinClusterNames_.end()
                    ? static_cast<uint32_t>(std::distance(dst.skinClusterNames_.begin(), nameIt))
                    : 0u;
            }

            auto &cluster = dst.skinClusters_[boneName];
            cluster.inverseBindPoseMatrix = ConvertMatrix(bone->mOffsetMatrix);

            cluster.vertexWeights.reserve(cluster.vertexWeights.size() + bone->mNumWeights);
            for (unsigned int wi = 0; wi < bone->mNumWeights; ++wi) {
                const aiVertexWeight &srcWeight = bone->mWeights[wi];
                const uint32_t vertexIndex = baseVertex + srcWeight.mVertexId;
                if (vertexIndex >= dst.vertices_.size()) continue;

                const float weight = srcWeight.mWeight;
                cluster.vertexWeights.push_back({ weight, vertexIndex });
                AddBoneInfluence(dst.vertices_[vertexIndex], boneIndex, weight);
            }
        }

        for (size_t i = baseVertex; i < dst.vertices_.size(); ++i) {
            NormalizeBoneWeights(dst.vertices_[i]);
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
    if (it == sFileNameToHandle.end()) {
        Log(Translation("engine.model.gethandle.failed.notfound") + fileName, LogSeverity::Warning);
        return kInvalidHandle;
    }
    return it->second;
}

ModelManager::ModelHandle ModelManager::GetModelHandleFromAssetPath(const std::string& assetPath) {
    LogScope scope;
    auto it = sAssetPathToHandle.find(NormalizePathSlashes(assetPath));
    if (it == sAssetPathToHandle.end()) {
        Log(Translation("engine.model.gethandle.failed.notfound") + assetPath, LogSeverity::Warning);
        return kInvalidHandle;
    }
    return it->second;
}

const ModelData &ModelManager::GetModelData(ModelHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) {
        Log(Translation("engine.model.getdata.failed.invalidhandle"), LogSeverity::Warning);
        return sEmptyData;
    }
    auto it = sModels.find(handle);
    if (it == sModels.end()) {
        Log(Translation("engine.model.getdata.failed.notfound") + std::to_string(handle), LogSeverity::Warning);
        return sEmptyData;
    }
    return it->second.data;
}

const ModelData &ModelManager::GetModelDataFromFileName(const std::string &fileName) {
    LogScope scope;
    const auto h = GetModelHandleFromFileName(fileName);
    if (h == kInvalidHandle) {
        Log(Translation("engine.model.getdata.failed.notfound") + fileName, LogSeverity::Warning);
        return sEmptyData;
    }
    return GetModelData(h);
}

const ModelData &ModelManager::GetModelDataFromAssetPath(const std::string &assetPath) {
    LogScope scope;
    const auto h = GetModelHandleFromAssetPath(assetPath);
    if (h == kInvalidHandle) {
        Log(Translation("engine.model.getdata.failed.notfound") + assetPath, LogSeverity::Warning);
        return sEmptyData;
    }
    return GetModelData(h);
}

#if defined(USE_IMGUI)
std::vector<ModelManager::ModelListEntry> ModelManager::GetLoadedModelListEntries() {
    return GetImGuiModelListEntries();
}

void ModelManager::ShowImGuiLoadedModelsWindow() {
    ImGui::Begin("ModelManager - Loaded Models");

    const auto entries = ModelManager::GetImGuiModelListEntries();
    ImGui::Text("Loaded Models: %d", static_cast<int>(entries.size()));

    static ImGuiTextFilter filter;
    filter.Draw("Filter");

    ImGui::Separator();

    if (ImGui::BeginTable("##ModelList", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, 300))) {
        ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("FileName");
        ImGui::TableSetupColumn("AssetPath");
        ImGui::TableSetupColumn("Vertices", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Indices", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableHeadersRow();

        for (const auto& e : entries) {
            if (filter.IsActive()) {
                if (!filter.PassFilter(e.fileName.c_str()) && !filter.PassFilter(e.assetPath.c_str())) {
                    continue;
                }
            }

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", e.handle);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(e.fileName.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(e.assetPath.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u", e.vertexCount);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", e.indexCount);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

std::vector<ModelManager::ModelHandle> ModelManager::GetAllImGuiModels() {
    LogScope scope;
    std::vector<ModelHandle> out;
    out.reserve(sModels.size());
    for (const auto &kv : sModels) out.push_back(kv.first);
    return out;
}

std::vector<ModelManager::ModelListEntry> ModelManager::GetImGuiModelListEntries() {
    LogScope scope;
    std::vector<ModelListEntry> out;
    out.reserve(sModels.size());

    for (const auto &kv : sModels) {
        const auto &m = kv.second;
        ModelListEntry e;
        e.handle = kv.first;
        e.fileName = m.fileName;
        e.assetPath = m.assetPath;
        e.vertexCount = m.data.GetVertexCount();
        e.indexCount = m.data.GetIndexCount();
        out.push_back(std::move(e));
    }

    std::sort(out.begin(), out.end(), [](const ModelListEntry &a, const ModelListEntry &b) {
        return a.assetPath < b.assetPath;
        });

    return out;
}
#endif

} // namespace KashipanEngine
