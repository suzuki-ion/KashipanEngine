#include "ModelManager.h"
#include "Assets/AssimpUtf8IOSystem.h"
#include "Assets/CaseInsensitive.h"
#include "Core/ProjectPaths.h"
#include "Assets/MaterialManager.h"
#include "Assets/PrimitiveMeshGenerator.h"
#include "Assets/TextureManager.h"

#include "Debug/Logger.h"
#include "Math/Quaternion.h"
#include "Math/Vector4.h"
#include "Utilities/Conversion/ConvertString.h"
#include "Utilities/FileIO/Directory.h"
#include "Utilities/FileIO/JSON.h"
#include "Utilities/Plugin/Plugins.h"
#include "Utilities/Translation.h"
#include "Utilities/UUID128.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#include <imgui_internal.h>
#include "Scene/Editor/PrefabAssetManager.h"
#include "Scene/SceneContext.h"
#include "Objects/EmptyObject.h"
#include "Objects/Components/Transform.h"
#include "Objects/Components/MeshFilter.h"
#include "Objects/Components/Render/Camera3D.h"
#include "Objects/Components/Render/CameraRenderer.h"
#include "Objects/Components/Render/Light.h"
#include "Objects/Components/Render/LightRenderer.h"
#include "Objects/Components/Render/MeshRenderer.h"
#include "Objects/Components/Render/ScreenBufferObject.h"
#include "Graphics/ScreenBuffer.h"
#include "Math/Quaternion.h"
#endif

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <functional>
#include <string_view>
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
    /// @brief dataの版数。UpdateProceduralMeshのたびに増加する（ResourceContainer側のGPUバッファ
    ///        キャッシュの再構築要否判定に使う。ModelManager::GetModelDataVersion参照）
    std::uint64_t dataVersion = 1;
};

std::unordered_map<Handle, ModelEntry> sModels;
FileMap<Handle> sFileNameToHandle;
FileMap<Handle> sAssetPathToHandle;

ModelManager* sActiveInstance = nullptr;

std::string NormalizePathSlashes(std::string s) {
    std::replace(s.begin(), s.end(), '\\', '/');
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
}

std::string ToLower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// @brief パス末尾のファイル名部分を文字列操作のみで取得する（std::filesystem::pathを介さない）
/// @details 埋め込みテクスチャの仮想パス（"モデルパス:埋め込み名"）はコロンを含むため、
///          std::filesystem::pathで再パースすると意図しない解釈をされる可能性がある
///          （Windowsのドライブレター/ADS記法との混同を避けるため、単純な文字列操作で切り出す）
std::string ExtractFileNameOnly(const std::string &path) {
    const auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

#if defined(USE_IMGUI)
/// @brief モデル自動生成Prefabを通常Prefabと同じID・親参照スキーマへ補完する
/// @details 既存のコンポーネントやユーザー編集値は変更せず、Prefab同期に必要なメタデータだけを追加する。
/// @return JSONを変更した場合はtrue
bool EnsureModelPrefabSchema(JSON &prefab, UUID128 &prefabID) {
    if (!prefab.is_object() || !prefab.contains("objects") || !prefab["objects"].is_array()) return false;

    bool changed = false;
    prefabID = UUID128(prefab.value("prefabID", std::string{}));
    if (!prefabID.IsValid()) {
        prefabID = UUID128(true);
        prefab["prefabID"] = prefabID.ToString();
        changed = true;
    }
    if (prefab.value("prefab", false) != true) {
        prefab["prefab"] = true;
        changed = true;
    }

    JSON &entries = prefab["objects"];
    for (size_t i = 0; i < entries.size(); ++i) {
        JSON &entry = entries[i];
        if (!entry.contains("object") || !entry["object"].is_object()) continue;
        JSON &object = entry["object"];

        const UUID128 nodeID(object.value("prefabNodeID", std::string{}));
        if (!nodeID.IsValid()) {
            object["prefabNodeID"] = UUID128(true).ToString();
            changed = true;
        }

        std::string expectedParentObjectID;
        const int parentIndex = entry.value("parentIndex", -1);
        if (parentIndex >= 0 && static_cast<size_t>(parentIndex) < entries.size()) {
            const JSON &parentEntry = entries[static_cast<size_t>(parentIndex)];
            if (parentEntry.contains("object")) {
                expectedParentObjectID = parentEntry["object"].value("objectID", std::string{});
            }
        }

        if (!object.contains("components") || !object["components"].is_array()) continue;
        for (auto &component : object["components"]) {
            if (component.value("type", std::string{}) != "Transform" || !component.contains("data")) continue;
            JSON &data = component["data"];
            if (!data.contains("customData") || !data["customData"].is_object()) {
                data["customData"] = JSON::object();
            }
            JSON &customData = data["customData"];
            const std::string currentParent = customData.value("parent", std::string{});
            if (expectedParentObjectID.empty()) {
                if (customData.erase("parent") != 0) changed = true;
            } else if (currentParent != expectedParentObjectID) {
                customData["parent"] = expectedParentObjectID;
                changed = true;
            }
            break;
        }
    }
    return changed;
}
#endif

bool HasSupportedModelExtension(const std::filesystem::path& p) {
    const std::string ext = ToLower(p.extension().string());
    return (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".3ds" || ext == ".blend" || ext == ".ply" || ext == ".stl" || ext == ".x");
}

std::string MakeAssetRelativePath(const std::string& assetsRoot, const std::string& fullPath) {
    const std::filesystem::path root = Utf8StringToPath(assetsRoot);
    const std::filesystem::path full = Utf8StringToPath(fullPath);

    std::error_code ec;
    auto rel = std::filesystem::relative(full, root, ec);
    if (ec) {
        return NormalizePathSlashes(PathToUtf8String(full.filename()));
    }
    return NormalizePathSlashes(PathToUtf8String(rel));
}

Matrix4x4 ConvertMatrix(const aiMatrix4x4& m) {
    // Assimpの行列は列ベクトル規約(v' = M * v)で定義されているが、本エンジンのMatrix4x4は
    // 行ベクトル規約(v' = v * M)で乗算するため、そのまま乗算に使えるよう転置して格納する
    // （SkeletonManager側はDecompose()でTRSへ分解してから本エンジン独自の規約で再構築しているため
    // この転置は不要だが、GPUスキニングではボーンオフセット行列をそのまま乗算に使うため必須）。
    Matrix4x4 out{};
    out.m[0][0] = m.a1; out.m[1][0] = m.a2; out.m[2][0] = m.a3; out.m[3][0] = m.a4;
    out.m[0][1] = m.b1; out.m[1][1] = m.b2; out.m[2][1] = m.b3; out.m[3][1] = m.b4;
    out.m[0][2] = m.c1; out.m[1][2] = m.c2; out.m[2][2] = m.c3; out.m[3][2] = m.c4;
    out.m[0][3] = m.d1; out.m[1][3] = m.d2; out.m[2][3] = m.d3; out.m[3][3] = m.d4;
    return out;
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

/// @brief ParseModelFileの結果一式（実体定義。ModelManager.hではopaqueな前方宣言のみ）
/// @details ワーカースレッドで作成された時点ではGPUリソース・グローバル状態には一切触れておらず、
///          FinalizeParsedModel（メインスレッド）で初めてそれらに反映される
struct ParsedModelResult {
    bool success = false;

    // RegisterEntryへ渡すためのModelEntry相当のデータ
    std::string fullPath;
    std::string assetPath;
    std::string fileName;
    ModelData data;

    // ノード分解・プレハブ生成（RegisterNodeDecompositionAndPrefab）に必要な情報
    std::unique_ptr<Assimp::Importer> importer; // scene の所有者。破棄するとsceneも無効になる
    const aiScene *scene = nullptr;
    std::vector<ModelData::MaterialData> materialsCopy;

    /// @brief 未アップロードの埋め込みテクスチャ（デコード済み生バイト列）
    struct PendingEmbeddedTexture {
        std::string registerPath;
        std::vector<uint8_t> data;
    };
    std::vector<PendingEmbeddedTexture> pendingTextures;
};

ModelManager::ModelManager(Passkey<GameEngine>, const std::string& assetsRootPath)
    : assetsRootPath_(NormalizePathSlashes(assetsRootPath)) {
    LogScope scope;
    sActiveInstance = this;
    PrimitiveMeshGenerator::RegisterBuiltinPrimitiveMeshes();
    LoadAllFromAssetsFolder();
}

ModelManager::~ModelManager() {
    LogScope scope;
    if (sActiveInstance == this) sActiveInstance = nullptr;
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

    // ファイルI/O・Assimp解析・メッシュ抽出はCPU処理のみでGPUリソース・グローバル状態に触れないため、
    // スレッドプールで並列実行する（各要素は担当するインデックス以外書き込まないためロック不要）
    std::vector<std::unique_ptr<ParsedModelResult>> parsedResults(files.size());
    Plugin::RunParallelAndWait(files.size(), [this, &files, &parsedResults](size_t i) {
        parsedResults[i] = ParseModelFile(files[i]);
        });

    // 全ファイルの解析が完了したら、メインスレッドでGPUアップロード・グローバル状態への登録・
    // プレハブ生成を順に行う
    for (auto& parsed : parsedResults) {
        FinalizeParsedModel(std::move(parsed));
    }
}

std::unique_ptr<ParsedModelResult> ModelManager::ParseModelFile(const std::string& filePath) const {
    LogScope scope;
    auto result = std::make_unique<ParsedModelResult>();
    if (filePath.empty()) return result;

    Log(Translation("engine.model.loading.start") + filePath, LogSeverity::Info);

    const std::filesystem::path p = Utf8StringToPath(filePath);

    if (!std::filesystem::exists(p)) {
        Log(Translation("engine.model.loading.failed.notfound") + PathToUtf8String(p), LogSeverity::Warning);
        return result;
    }
    if (!HasSupportedModelExtension(p)) {
        Log(Translation("engine.model.loading.failed.unsupported") + PathToUtf8String(p), LogSeverity::Warning);
        return result;
    }

    result->importer = std::make_unique<Assimp::Importer>();
    // Assimpの既定IOSystemはWindows上でfopen（現在のANSIコードページ）を使うため、
    // コードページで表現できない文字を含むパス（多言語のファイル名等）を開けない。
    // _wfopenベースの独自IOSystemに差し替えることで回避する（Importerが所有権を持つ）
    result->importer->SetIOHandler(new AssimpUtf8IOSystem());
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
        // 法線を持たない（または元データが不正で aiProcess_FindInvalidData により
        // 取り除かれた）メッシュが、法線ゼロベクトルのまま描画されてライティング結果が
        // 常に真っ黒になる問題を防ぐ。法線を既に持つメッシュには影響しない
        // （Assimpは既存の法線がある場合この処理をスキップする）
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_FlipUVs;

    const aiScene* scene = result->importer->ReadFile(PathToUtf8String(p), flags);
    if (!scene || !scene->mRootNode) {
        Log(Translation("engine.model.loading.failed.assimp") + PathToUtf8String(p) + " msg=" + result->importer->GetErrorString(), LogSeverity::Warning);
        result->importer.reset();
        return result;
    }
    result->scene = scene;

    result->fullPath = NormalizePathSlashes(PathToUtf8String(p));
    result->assetPath = MakeAssetRelativePath(assetsRootPath_, result->fullPath);
    result->fileName = PathToUtf8String(p.filename());

    // Set asset relative path in ModelData
    result->data.assetRelativePath_ = result->assetPath;

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
                // 外部ファイル参照（モデルファイルからの相対パス）
                const std::filesystem::path texFull = p.parent_path() / Utf8StringToPath(tp);
                md.diffuseTexturePath = MakeAssetRelativePath(assetsRootPath_, NormalizePathSlashes(PathToUtf8String(texFull)));
            } else if (!tp.empty()) {
                // 埋め込みテクスチャ（bufferView経由。PMX→glTF変換等で全テクスチャが
                // 1つのバイナリにまとめられている場合によく使われる）。
                // "*N"はscene->mTextures[N]（またはGetEmbeddedTexture）への参照を表す
                // ここではGPUアップロードは行わず、生バイト列のコピーだけを保持する
                // （TextureManagerへの登録はメインスレッドのFinalizeParsedModelで行う）
                if (const aiTexture *embeddedTex = scene->GetEmbeddedTexture(tp.c_str())) {
                    // mHeight==0の場合はpcDataが圧縮画像（PNG/JPEG等）の生バイト列で、
                    // mWidthがそのバイト数（mHeight!=0の生ARGB8888データは非対応）
                    if (embeddedTex->mHeight == 0 && embeddedTex->pcData) {
                        const std::string embeddedName = embeddedTex->mFilename.length > 0
                            ? std::string(embeddedTex->mFilename.C_Str())
                            : ("EmbeddedTexture" + std::to_string(mi));
                        const std::string registerPath = NormalizePathSlashes(PathToUtf8String(p)) + ":" + embeddedName;

                        ParsedModelResult::PendingEmbeddedTexture pending;
                        pending.registerPath = registerPath;
                        const auto *bytes = reinterpret_cast<const uint8_t *>(embeddedTex->pcData);
                        pending.data.assign(bytes, bytes + embeddedTex->mWidth);
                        result->pendingTextures.push_back(std::move(pending));

                        md.diffuseTexturePath = registerPath;
                    }
                }
            }
        }
        result->data.materials_.push_back(std::move(md));
    }

    std::function<void(const aiNode*)> appendNodeMeshes;
    appendNodeMeshes = [&](const aiNode* node) {
        if (!node) return;
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const unsigned int meshIdx = node->mMeshes[i];
            if (meshIdx >= scene->mNumMeshes) continue;
            AppendMeshToModelData(scene->mMeshes[meshIdx], result->data);
        }
        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            appendNodeMeshes(node->mChildren[c]);
        }
    };

    appendNodeMeshes(scene->mRootNode);

    if (result->data.GetVertexCount() == 0 || result->data.GetIndexCount() == 0) {
        Log(Translation("engine.model.loading.failed.nomesh") + PathToUtf8String(p), LogSeverity::Warning);
        result->importer.reset();
        result->scene = nullptr;
        return result;
    }

    result->materialsCopy = result->data.materials_;
    result->success = true;
    return result;
}

ModelManager::ModelHandle ModelManager::FinalizeParsedModel(std::unique_ptr<ParsedModelResult> parsed) {
    LogScope scope;
    if (!parsed || !parsed->success) return kInvalidHandle;

    // 埋め込みテクスチャのGPUアップロード（DirectX12リソース確保を伴うためメインスレッドで行う）
    for (const auto& pending : parsed->pendingTextures) {
        if (auto *textureManager = TextureManager::GetActiveInstance(Passkey<ModelManager>{})) {
            textureManager->RegisterTextureFromMemory(pending.registerPath, pending.data.data(), pending.data.size());
        }
    }

    ModelEntry entry{};
    entry.fullPath = parsed->fullPath;
    entry.assetPath = parsed->assetPath;
    entry.fileName = parsed->fileName;
    entry.data = std::move(parsed->data);

    // RegisterEntryでentryがムーブされるため、ノード分解・プレハブ生成に使う情報を先に控えておく
    const std::string modelFullPath = parsed->fullPath;
    const std::string baseAssetPath = parsed->assetPath;
    const std::string baseFileName = parsed->fileName;
    const std::vector<ModelData::MaterialData> materialsCopy = std::move(parsed->materialsCopy);
    const aiScene* scene = parsed->scene;

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.model.loading.failed.register") + modelFullPath, LogSeverity::Error);
        return kInvalidHandle;
    }

    // ノード分解によるサブメッシュ登録と、モデル階層のプレハブ自動生成
    // （parsed->importerがsceneを所有し続けているため、この時点でもsceneは有効）
    RegisterNodeDecompositionAndPrefab(scene, modelFullPath, baseAssetPath, baseFileName, materialsCopy);

    Log(Translation("engine.model.loading.succeeded") + modelFullPath, LogSeverity::Info);
    return handle;
}

ModelManager::ModelHandle ModelManager::LoadModel(const std::string& filePath) {
    LogScope scope;
    if (filePath.empty()) return kInvalidHandle;

    {
        const std::string normalized = NormalizePathSlashes(filePath);
        auto it = sAssetPathToHandle.find(normalized);
        if (it != sAssetPathToHandle.end()) {
            Log(Translation("engine.model.loading.alreadyloaded") + normalized, LogSeverity::Debug);
            return it->second;
        }
    }

    return FinalizeParsedModel(ParseModelFile(filePath));
}

// ModelData は private メンバを持つため、friend である ModelManager のメンバ関数として実装する
void ModelManager::AppendMeshToModelData(const aiMesh* mesh, ModelData& dst) {
    LogScope scope;
    {
        if (!mesh) return;
        // （元はLoadModel内のローカルラムダ。インデントを維持するためブロックで囲っている）

        const bool hasNormals = mesh->HasNormals();
        const bool hasUV0 = mesh->HasTextureCoords(0);
        const bool hasTangents = mesh->HasTangentsAndBitangents();

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

            // タンジェントが無いメッシュ（UV0が無い等）はModelData::Vertexの既定値のまま
            // （法線マップ非使用のマテリアルでは参照されないため実害は無い）
            if (hasTangents) {
                v.tx = mesh->mTangents[i].x;
                v.ty = mesh->mTangents[i].y;
                v.tz = mesh->mTangents[i].z;
            }

            dst.vertices_.push_back(v);
        }

        // サブメッシュ範囲を記録する（aiMeshは必ず1メッシュ＝1マテリアルのため、
        // 追加するメッシュ1つがそのまま1サブメッシュになる）
        ModelData::SubMesh subMesh;
        subMesh.indexStart = static_cast<uint32_t>(dst.indices_.size());
        subMesh.materialIndex = mesh->mMaterialIndex;

        for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                dst.indices_.push_back(baseVertex + face.mIndices[j]);
            }
        }
        subMesh.indexCount = static_cast<uint32_t>(dst.indices_.size()) - subMesh.indexStart;
        if (subMesh.indexCount > 0) {
            dst.subMeshes_.push_back(subMesh);
        }

        // ボーン（スキンウェイト）の抽出。SkinnedMeshRenderer のGPUスキニングで使用する
        for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi) {
            const aiBone* bone = mesh->mBones[bi];
            if (!bone) continue;
            const std::string boneName = bone->mName.C_Str();
            if (boneName.empty()) continue;

            auto& cluster = dst.skinClusters_[boneName];
            cluster.inverseBindPoseMatrix = ConvertMatrix(bone->mOffsetMatrix);
            cluster.vertexWeights.reserve(cluster.vertexWeights.size() + bone->mNumWeights);
            for (unsigned int wi = 0; wi < bone->mNumWeights; ++wi) {
                const auto& w = bone->mWeights[wi];
                ModelData::VertexWeightData vw;
                vw.weight = w.mWeight;
                vw.vertexIndex = baseVertex + w.mVertexId;
                cluster.vertexWeights.push_back(vw);
            }
        }

        // BlendShape（モーフターゲット）の頂点差分抽出。SkinnedMeshRenderer のGPUスキニングで使用する
        for (unsigned int ai = 0; ai < mesh->mNumAnimMeshes; ++ai) {
            const aiAnimMesh* animMesh = mesh->mAnimMeshes[ai];
            if (!animMesh) continue;
            // 名前無しの場合の連番はメッシュ内ローカル(ai)ではなくdst全体の通し番号にする。
            // aiのままだと複数サブメッシュそれぞれの1個目が"BlendShape0"のように衝突し、
            // 下の同名マージ処理で本来別々のBlendShapeが誤って1つに統合されてしまう
            const std::string shapeName = animMesh->mName.length > 0
                ? std::string(animMesh->mName.C_Str())
                : ("BlendShape" + std::to_string(dst.blendShapes_.size()));

            // 同名のBlendShapeが既にあれば追記する（複数メッシュに分かれたモデル用）
            ModelData::BlendShapeData* target = nullptr;
            for (auto& bs : dst.blendShapes_) {
                if (bs.name == shapeName) {
                    target = &bs;
                    break;
                }
            }
            if (!target) {
                dst.blendShapes_.push_back(ModelData::BlendShapeData{});
                target = &dst.blendShapes_.back();
                target->name = shapeName;
            }

            const bool hasAnimNormals = animMesh->HasNormals() && hasNormals;
            const unsigned int animVertexCount = std::min(animMesh->mNumVertices, mesh->mNumVertices);
            constexpr float kEpsilon = 1e-6f;
            for (unsigned int vi = 0; vi < animVertexCount; ++vi) {
                const aiVector3D& basePos = mesh->mVertices[vi];
                const aiVector3D& morphPos = animMesh->mVertices[vi];
                Vector3 deltaPos(morphPos.x - basePos.x, morphPos.y - basePos.y, morphPos.z - basePos.z);
                Vector3 deltaNormal(0.0f, 0.0f, 0.0f);
                if (hasAnimNormals) {
                    const aiVector3D& baseNormal = mesh->mNormals[vi];
                    const aiVector3D& morphNormal = animMesh->mNormals[vi];
                    deltaNormal = Vector3(morphNormal.x - baseNormal.x, morphNormal.y - baseNormal.y, morphNormal.z - baseNormal.z);
                }
                if (std::abs(deltaPos.x) < kEpsilon && std::abs(deltaPos.y) < kEpsilon && std::abs(deltaPos.z) < kEpsilon &&
                    std::abs(deltaNormal.x) < kEpsilon && std::abs(deltaNormal.y) < kEpsilon && std::abs(deltaNormal.z) < kEpsilon) {
                    continue;
                }
                ModelData::BlendShapeVertexDelta delta;
                delta.deltaPosition = deltaPos;
                delta.deltaNormal = deltaNormal;
                delta.vertexIndex = baseVertex + vi;
                target->deltas.push_back(delta);
            }
        }
    }
}

ModelManager::ModelHandle ModelManager::RegisterProceduralMesh(const std::string& name, std::vector<ModelData::Vertex> vertices, std::vector<std::uint32_t> indices) {
    LogScope scope;
    const std::string normalizedName = NormalizePathSlashes(name);

    // 同名で登録済みの場合は再登録せず既存のハンドルを返す
    auto it = sAssetPathToHandle.find(normalizedName);
    if (it != sAssetPathToHandle.end()) return it->second;

    ModelEntry entry{};
    entry.fullPath = normalizedName;
    entry.assetPath = normalizedName;
    entry.fileName = normalizedName;
    entry.data.assetRelativePath_ = normalizedName;
    entry.data.vertices_ = std::move(vertices);
    entry.data.indices_ = std::move(indices);

    const auto handle = RegisterEntry(std::move(entry));
    if (handle == kInvalidHandle) {
        Log(Translation("engine.model.loading.failed.register") + normalizedName, LogSeverity::Error);
        return kInvalidHandle;
    }
    return handle;
}

bool ModelManager::UpdateProceduralMesh(ModelHandle handle, std::vector<ModelData::Vertex> vertices, std::vector<std::uint32_t> indices) {
    LogScope scope;
    if (handle == kInvalidHandle) return false;
    auto it = sModels.find(handle);
    if (it == sModels.end()) return false;

    ModelEntry &entry = it->second;
    entry.data.vertices_ = std::move(vertices);
    entry.data.indices_ = std::move(indices);
    ++entry.dataVersion;
    return true;
}

std::uint64_t ModelManager::GetModelDataVersion(ModelHandle handle) {
    LogScope scope;
    if (handle == kInvalidHandle) return 0;
    auto it = sModels.find(handle);
    if (it == sModels.end()) return 0;
    return it->second.dataVersion;
}

#if defined(USE_IMGUI)
ModelManager::ModelHandle ModelManager::LoadModelDynamic(const std::string &filePath) {
    LogScope scope;
    if (!sActiveInstance) return kInvalidHandle;
    return sActiveInstance->LoadModel(filePath);
}
#endif

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

std::string ModelManager::GetBaseAssetPath(const std::string& assetPath) {
    LogScope scope;
    // サブメッシュのアセットパスは "モデルパス:ノード名" 形式（アセットパスはAssetsルートからの
    // 相対パスのためドライブレターの ":" は含まれない）
    const auto pos = assetPath.find(':');
    return (pos == std::string::npos) ? assetPath : assetPath.substr(0, pos);
}

void ModelManager::RegisterNodeDecompositionAndPrefab(const aiScene *scene,
    const std::string &modelFullPath, const std::string &baseAssetPath,
    const std::string &baseFileName, const std::vector<ModelData::MaterialData> &materials) {
    LogScope scope;
    if (!scene || !scene->mRootNode) return;

    // メッシュを持つノードの情報（サブメッシュのアセットパス・スキニング有無・使用マテリアル）
    struct MeshNodeInfo {
        std::string meshAssetPath;
        bool skinned = false;
        /// @brief ノード内の各メッシュ（＝サブメッシュ）が使用するマテリアル番号（追加順）
        std::vector<unsigned int> materialIndices;
    };
    std::unordered_map<const aiNode *, MeshNodeInfo> meshNodeInfos;

    // メッシュを持つノードを収集する（ノード名はモデル内で一意になるよう連番を付ける）
    std::vector<const aiNode *> meshNodes;
    std::unordered_map<std::string, int> nameCounts;
    std::unordered_map<const aiNode *, std::string> uniqueNames;
    std::function<void(const aiNode *)> collect = [&](const aiNode *node) {
        if (!node) return;
        if (node->mNumMeshes > 0) {
            std::string name = node->mName.length > 0 ? node->mName.C_Str() : "Mesh";
            const int count = ++nameCounts[name];
            if (count > 1) name += "_" + std::to_string(count);
            uniqueNames[node] = name;
            meshNodes.push_back(node);

            MeshNodeInfo info;
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                const unsigned int meshIdx = node->mMeshes[i];
                if (meshIdx >= scene->mNumMeshes || !scene->mMeshes[meshIdx]) continue;
                info.materialIndices.push_back(scene->mMeshes[meshIdx]->mMaterialIndex);
                if (scene->mMeshes[meshIdx]->mNumBones > 0) info.skinned = true;
            }
            meshNodeInfos[node] = std::move(info);
        }
        for (unsigned int c = 0; c < node->mNumChildren; ++c) collect(node->mChildren[c]);
    };
    collect(scene->mRootNode);
    if (meshNodes.empty()) return;

    if (meshNodes.size() >= 2) {
        // 複数のメッシュノードを持つ場合、ノード単位のサブメッシュを "モデルパス:ノード名" で個別登録する
        // （Unityのモデル内サブアセットに相当。MeshFilterのMesh選択やプレハブから個別に参照できる）
        for (const aiNode *node : meshNodes) {
            const std::string &uniqueName = uniqueNames[node];
            ModelEntry subEntry{};
            subEntry.fullPath = modelFullPath + ":" + uniqueName;
            subEntry.assetPath = baseAssetPath + ":" + uniqueName;
            subEntry.fileName = baseFileName + ":" + uniqueName;
            subEntry.data.assetRelativePath_ = subEntry.assetPath;
            subEntry.data.materials_ = materials;
            for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                const unsigned int meshIdx = node->mMeshes[i];
                if (meshIdx >= scene->mNumMeshes) continue;
                AppendMeshToModelData(scene->mMeshes[meshIdx], subEntry.data);
            }
            if (subEntry.data.GetVertexCount() == 0) continue;
            const std::string subAssetPath = subEntry.assetPath;
            if (RegisterEntry(std::move(subEntry)) != kInvalidHandle) {
                meshNodeInfos[node].meshAssetPath = subAssetPath;
            }
        }
    } else {
        // メッシュノードが1つだけの場合はモデル全体のメッシュ（既に登録済み）をそのまま参照する
        meshNodeInfos[meshNodes.front()].meshAssetPath = baseAssetPath;
    }

#if defined(USE_IMGUI)
    const std::string modelName = PathToUtf8String(Utf8StringToPath(baseFileName).stem());

    // モデルが使用するマテリアルを自動生成する（.matファイルをモデルの隣へ書き出す）。
    // MaterialManagerの起動時Assetsフォルダ走査を待たず、書き出した直後にLoadMaterialDynamicで
    // 実行中のMaterialManagerへ即時登録する（そうしないとAssetsウィンドウでの
    // ダブルクリック等、生成直後のセッション内参照が「読み込まれていない」扱いになる）。
    // 既に.matファイルが存在する場合はユーザーの編集を保護するため上書きしないが、
    // 登録自体は（未登録なら）行う
    const std::string modelDir = NormalizePathSlashes(PathToUtf8String(Utf8StringToPath(modelFullPath).parent_path()));
    std::unordered_map<unsigned int, std::string> materialIndexToName;
    for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
        aiString aiName;
        std::string matName;
        if (scene->mMaterials[mi] && AI_SUCCESS == scene->mMaterials[mi]->Get(AI_MATKEY_NAME, aiName)) {
            matName = aiName.C_Str();
        }
        if (matName.empty()) matName = "Material" + std::to_string(mi);
        // ファイル名に使えない文字を除去し、モデル名を前置してモデル間の名前重複を避ける
        constexpr std::string_view kInvalidChars = "\\/:*?\"<>|";
        for (auto &c : matName) {
            if (kInvalidChars.find(c) != std::string_view::npos) c = '_';
        }
        const std::string uniqueName = modelName + "_" + matName;
        materialIndexToName[mi] = uniqueName;

        const std::string matFilePath = modelDir + "/" + uniqueName + ".mat";
        std::error_code matEc;
        if (std::filesystem::exists(Utf8StringToPath(matFilePath), matEc)) {
            MaterialManager::LoadMaterialDynamic(matFilePath);
            continue;
        }

        // MaterialManager::SaveMaterialと同じスキーマで書き出す
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        std::string textureFile;
        if (mi < materials.size()) {
            color = Vector4(materials[mi].baseColor[0], materials[mi].baseColor[1],
                materials[mi].baseColor[2], materials[mi].baseColor[3]);
            if (!materials[mi].diffuseTexturePath.empty()) {
                // MaterialManagerのtextureFileはファイル名単体で管理される
                textureFile = ExtractFileNameOnly(materials[mi].diffuseTexturePath);
            }
        }
        JSON matJson = JSON::object();
        matJson["name"] = uniqueName;
        matJson["color"] = ToJSON(color);
        matJson["uvTranslate"] = ToJSON(Vector2{ 0.0f, 0.0f });
        matJson["uvRotation"] = 0.0f;
        matJson["uvScale"] = ToJSON(Vector2{ 1.0f, 1.0f });
        matJson["uvPivot"] = ToJSON(Vector2{ 0.5f, 0.5f });
        matJson["textureFile"] = textureFile;
        matJson["environmentFile"] = "";
        matJson["samplerHandle"] = static_cast<std::uint32_t>(0);
        matJson["shininess"] = 32.0f;
        matJson["specularColor"] = ToJSON(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        matJson["environmentCoefficient"] = 1.0f;
        matJson["enableLighting"] = true;
        matJson["enableShadowMapProjection"] = true;
        if (SaveJSON(matJson, matFilePath)) {
            Log(Translation("engine.model.material.generated") + matFilePath, LogSeverity::Info);
            MaterialManager::LoadMaterialDynamic(matFilePath);
        }
    }

    // 生成・登録した.matアセット名を、既にsModelsへ登録済みのModelData（ベースモデルと
    // 各ノード単位のサブメッシュ）へ書き戻す。デバッグプレビュー等が本来のマテリアルを
    // 引き当てられるようにするため（RegisterEntryは本関数より前に完了しているため、
    // materials_はここで直接パッチする必要がある）
    const auto patchMaterialAssetNames = [&materialIndexToName](const std::string &assetPath) {
        if (assetPath.empty()) return;
        const auto handle = GetModelHandleFromAssetPath(assetPath);
        const auto it = sModels.find(handle);
        if (it == sModels.end()) return;
        auto &mats = it->second.data.materials_;
        for (const auto &[mi, name] : materialIndexToName) {
            if (mi < mats.size()) mats[mi].materialAssetName = name;
        }
    };
    patchMaterialAssetNames(baseAssetPath);
    for (const aiNode *node : meshNodes) {
        patchMaterialAssetNames(meshNodeInfos[node].meshAssetPath);
    }

    // エディタービルドでは、ノード階層（アーマチュア・メッシュオブジェクト）を再現するプレハブを
    // モデルの隣（"モデルファイル名.prefab"）へ自動生成する。
    // 既にプレハブが存在する場合はユーザーの編集を保護し、同期に必要なID・親参照だけを補完する
    const std::string prefabPath = modelFullPath + ".prefab";
    // PrefabAssetManagerの索引・保存APIは論理パス（"Assets/..."）を前提にしている
    // （EnsureIndexBuildedはProjectPaths::ListAssetFilesの論理パスで索引を構築するため、
    //   ここで物理パスをそのまま渡すと同じprefabIDのエントリが物理パスキーで上書きされ、
    //   AssetsウィンドウからのD&D配置（論理パスでGetPrefabIDFromPathを引く）が
    //   PrefabIDを解決できずPrefabInstanceComponentが付与されない＝同期が機能しなくなる）
    const std::string prefabLogicalPath = ProjectPaths::ToLogical(prefabPath);
    std::error_code ec;
    if (std::filesystem::exists(prefabPath, ec)) {
        JSON existingPrefab = LoadJSON(prefabPath);
        UUID128 existingPrefabID;
        if (EnsureModelPrefabSchema(existingPrefab, existingPrefabID)) {
            if (PrefabAssetManager::CreatePrefabFile(existingPrefabID, existingPrefab, prefabLogicalPath)) {
                Log(Translation("engine.model.prefab.metadata.upgraded") + prefabPath, LogSeverity::Info);
            } else {
                Log(Translation("engine.model.prefab.metadata.upgrade.failed") + prefabPath, LogSeverity::Warning);
            }
        }
        return;
    }

    const UUID128 prefabID(true);
    JSON prefab;
    prefab["prefab"] = true;
    prefab["prefabID"] = prefabID.ToString();
    prefab["name"] = modelName;
    prefab["objects"] = JSON::array();

    // コンポーネント1つ分のJSON（EmptyObject::SaveToJsonが書き出す形式と同じ）を構築する
    const auto makeComponentJson = [](const char *type, int priority, JSON customData) {
        JSON data;
        data["priority"] = priority;
        data["isActive"] = true;
        data["tag"] = "";
        data["customData"] = std::move(customData);
        JSON comp;
        comp["type"] = type;
        comp["data"] = std::move(data);
        return comp;
    };

    std::function<void(const aiNode *, int, const std::string &, const std::string &)> addNode =
        [&](const aiNode *node, int parentIndex, const std::string &parentObjectID,
            const std::string &nameOverride) {
        if (!node) return;
        const int myIndex = static_cast<int>(prefab["objects"].size());

        // ノードのローカル変換をTRSへ分解してTransformにする（SkeletonManagerと同じ変換規約）
        aiVector3D scaling;
        aiQuaternion rotation;
        aiVector3D translation;
        node->mTransformation.Decompose(scaling, rotation, translation);
        JSON transformData;
        transformData["translate"] = ToJSON(Vector3(translation.x, translation.y, translation.z));
        transformData["rotate"] = ToJSON(Quaternion(rotation.x, rotation.y, rotation.z, rotation.w));
        transformData["scale"] = ToJSON(Vector3(scaling.x, scaling.y, scaling.z));
        if (!parentObjectID.empty()) transformData["parent"] = parentObjectID;

        JSON object;
        object["name"] = nameOverride.empty()
            ? (node->mName.length > 0 ? std::string(node->mName.C_Str()) : std::string("Node"))
            : nameOverride;
        object["tag"] = "";
        object["isActive"] = true;
        object["editorOnly"] = false;
        const std::string objectID = UUID128(true).ToString();
        object["objectID"] = objectID;
        object["prefabNodeID"] = UUID128(true).ToString();
        object["components"] = JSON::array();
        object["components"].push_back(makeComponentJson("Transform", 1, std::move(transformData)));

        // メッシュを持つノードにはMeshFilterと描画コンポーネントを付与する
        // （スキニングメッシュの場合はSkinnedMeshRenderer、それ以外はMeshRenderer）
        if (auto it = meshNodeInfos.find(node); it != meshNodeInfos.end() && !it->second.meshAssetPath.empty()) {
            JSON meshFilterData;
            meshFilterData["meshAssetPath"] = it->second.meshAssetPath;
            object["components"].push_back(makeComponentJson("MeshFilter", 1, std::move(meshFilterData)));
            // 自動生成したマテリアルを適用した状態にしておく
            // （サブメッシュ＝ノード内のメッシュごとにマテリアルスロットへ割り当てる）
            JSON rendererData = JSON::object();
            JSON materialNamesJson = JSON::array();
            for (const unsigned int matIndex : it->second.materialIndices) {
                auto matIt = materialIndexToName.find(matIndex);
                materialNamesJson.push_back(matIt != materialIndexToName.end() ? matIt->second : std::string("Default"));
            }
            if (!materialNamesJson.empty()) {
                // 後方互換のため単一の"materialName"（スロット0）も併記する
                rendererData["materialName"] = materialNamesJson.front();
                rendererData["materialNames"] = std::move(materialNamesJson);
            }
            object["components"].push_back(makeComponentJson(
                it->second.skinned ? "SkinnedMeshRenderer" : "MeshRenderer", 900, std::move(rendererData)));

            // スキニングメッシュの場合、アニメーション再生を担うAnimatorも併せて付与する
            // （SkinnedMeshRendererはAnimatorが解決したスケルトン姿勢を読み取るだけで、
            //   アニメーションクリップの選択・再生状態そのものは持たない）
            if (it->second.skinned) {
                JSON animatorData = JSON::object();
                animatorData["rootBoneObjectID"] = "";
                animatorData["clipName"] = "";
                animatorData["animationSourceAssetPath"] = "";
                animatorData["playOnStart"] = true;
                animatorData["loop"] = true;
                animatorData["playbackSpeed"] = 1.0;
                object["components"].push_back(makeComponentJson("Animator", 500, std::move(animatorData)));
            }
        }

        JSON entryJson;
        entryJson["parentIndex"] = parentIndex;
        entryJson["object"] = std::move(object);
        prefab["objects"].push_back(std::move(entryJson));

        for (unsigned int c = 0; c < node->mNumChildren; ++c) {
            addNode(node->mChildren[c], myIndex, objectID, std::string());
        }
    };
    // ルートノードはモデル名のオブジェクトとして生成する（Unityのモデルプレハブと同様）
    addNode(scene->mRootNode, -1, std::string(), modelName);

    if (PrefabAssetManager::CreatePrefabFile(prefabID, prefab, prefabLogicalPath)) {
        Log(Translation("engine.model.prefab.generated") + prefabPath, LogSeverity::Info);
    } else {
        Log(Translation("engine.model.prefab.generate.failed") + prefabPath, LogSeverity::Warning);
    }
#endif // USE_IMGUI
}

bool ModelManager::RenameModel(const std::string &oldAssetPath, const std::string &newAssetPath) {
    LogScope scope;
    const std::string normalizedOld = NormalizePathSlashes(oldAssetPath);
    auto pathIt = sAssetPathToHandle.find(normalizedOld);
    if (pathIt == sAssetPathToHandle.end()) return false;
    const Handle handle = pathIt->second;
    auto entryIt = sModels.find(handle);
    if (entryIt == sModels.end()) return false;

    ModelEntry &entry = entryIt->second;
    sAssetPathToHandle.erase(pathIt);
    sFileNameToHandle.erase(entry.fileName);

    const std::string normalizedNew = NormalizePathSlashes(newAssetPath);
    entry.assetPath = normalizedNew;
    entry.fileName = std::filesystem::path(normalizedNew).filename().string();
    entry.fullPath = ProjectPaths::AssetsRoot() + "/" + normalizedNew;
    entry.data.assetRelativePath_ = normalizedNew;

    sAssetPathToHandle[normalizedNew] = handle;
    sFileNameToHandle[entry.fileName] = handle;
    return true;
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
    LogScope scope;
    return GetImGuiModelListEntries();
}

namespace {

/// @brief デバッグ用モデルプレビューが生成する非表示・非保存オブジェクト一式のID
/// @details 生ポインタはフレームをまたいで保持せず、毎回SceneContext::GetSceneObjectで
///          UUIDから引き直す（Play開始・シーン再読み込みでオブジェクトが失われ得るため）
struct ModelPreviewObjects final {
    UUID128 targetObjectID{};
    UUID128 cameraObjectID{};
    UUID128 lightObjectID{};
    UUID128 meshObjectID{};
    bool valid = false;
};

/// @brief プレビューのオービットカメラ・選択状態（ウィンドウを開いている間、フレームをまたいで保持する）
struct ModelPreviewState final {
    ModelManager::ModelHandle selectedHandle = ModelManager::kInvalidHandle;
    /// @brief 直近でプレビュー用メッシュへ反映済みのハンドル（selectedHandleと異なれば再適用する）
    ModelManager::ModelHandle appliedHandle = ModelManager::kInvalidHandle;
    float yaw = 0.6f;
    float pitch = 0.35f;
    float distance = 5.0f;
    Vector3 target{ 0.0f, 0.0f, 0.0f };
};

constexpr std::uint32_t kModelPreviewBufferSize = 512;

ModelPreviewObjects &GetModelPreviewObjects() {
    static ModelPreviewObjects objects;
    return objects;
}

ModelPreviewState &GetModelPreviewState() {
    static ModelPreviewState state;
    return state;
}

/// @brief プレビュー用オブジェクト一式を破棄する
void DestroyModelPreviewObjects(SceneContext *sceneContext) {
    auto &objects = GetModelPreviewObjects();
    if (sceneContext && objects.valid) {
        for (const UUID128 id : { objects.meshObjectID, objects.lightObjectID, objects.cameraObjectID, objects.targetObjectID }) {
            if (auto *obj = sceneContext->GetSceneObject(id)) sceneContext->DeleteObject(obj);
        }
    }
    objects = ModelPreviewObjects{};
    GetModelPreviewState().appliedHandle = ModelManager::kInvalidHandle;
}

/// @brief プレビュー用オブジェクト一式が有効な状態で存在することを保証する（Play中は生成しない）
bool EnsureModelPreviewObjects(SceneContext *sceneContext) {
    auto &objects = GetModelPreviewObjects();
    if (!sceneContext) return false;
    if (objects.valid) {
        if (sceneContext->GetSceneObject(objects.targetObjectID) && sceneContext->GetSceneObject(objects.cameraObjectID) &&
            sceneContext->GetSceneObject(objects.lightObjectID) && sceneContext->GetSceneObject(objects.meshObjectID)) {
            return true;
        }
        // シーン再読み込み等で一部が失われている場合は作り直す
        objects = ModelPreviewObjects{};
    }
    if (sceneContext->IsPlaying()) return false;

    auto *targetObj = sceneContext->CreateEmptyObject("(Model Preview Target)");
    if (!targetObj) return false;
    targetObj->SetSaveEnabled(false);
    if (auto *screenBufferObject = targetObj->AddComponent<ScreenBufferObject>()) {
        screenBufferObject->SetName("ModelPreview");
        screenBufferObject->SetSize(kModelPreviewBufferSize, kModelPreviewBufferSize);
    }

    auto *cameraObj = sceneContext->CreateEmptyObject("(Model Preview Camera)");
    if (!cameraObj) { sceneContext->DeleteObject(targetObj); return false; }
    cameraObj->SetSaveEnabled(false);
    cameraObj->AddComponent<Camera3D>();
    if (auto *cameraRenderer = cameraObj->AddComponent<CameraRenderer>()) {
        cameraRenderer->SetTargetObject(targetObj);
    }

    auto *lightObj = sceneContext->CreateEmptyObject("(Model Preview Light)");
    if (!lightObj) { sceneContext->DeleteObject(cameraObj); sceneContext->DeleteObject(targetObj); return false; }
    lightObj->SetSaveEnabled(false);
    if (auto *transform = lightObj->GetComponent<Transform>()) {
        transform->SetRotate(Vector3(0.9f, -0.6f, 0.0f));
    }
    lightObj->AddComponent<Light>();
    if (auto *lightRenderer = lightObj->AddComponent<LightRenderer>()) {
        lightRenderer->SetTargetObject(targetObj);
    }

    auto *meshObj = sceneContext->CreateEmptyObject("(Model Preview Mesh)");
    if (!meshObj) { sceneContext->DeleteObject(lightObj); sceneContext->DeleteObject(cameraObj); sceneContext->DeleteObject(targetObj); return false; }
    meshObj->SetSaveEnabled(false);
    // 通常のScene Viewには映り込ませない（SceneRenderer::CollectSortableEntries参照）
    meshObj->SetHiddenFromEditorTarget(true);
    meshObj->AddComponent<MeshFilter>();
    if (auto *meshRenderer = meshObj->AddComponent<MeshRenderer>()) {
        meshRenderer->SetTargetObject(targetObj);
    }

    objects.targetObjectID = targetObj->GetObjectID();
    objects.cameraObjectID = cameraObj->GetObjectID();
    objects.lightObjectID = lightObj->GetObjectID();
    objects.meshObjectID = meshObj->GetObjectID();
    objects.valid = true;
    GetModelPreviewState().appliedHandle = ModelManager::kInvalidHandle;
    return true;
}

/// @brief モデルの全頂点から求めたAABB
struct ModelBounds final {
    Vector3 min{ 0.0f, 0.0f, 0.0f };
    Vector3 max{ 0.0f, 0.0f, 0.0f };
    bool valid = false;
};
ModelBounds ComputeModelBounds(const ModelData &data) {
    ModelBounds bounds;
    for (const auto &v : data.GetVertices()) {
        const Vector3 p(v.px, v.py, v.pz);
        if (!bounds.valid) {
            bounds.min = bounds.max = p;
            bounds.valid = true;
            continue;
        }
        bounds.min.x = std::min(bounds.min.x, p.x);
        bounds.min.y = std::min(bounds.min.y, p.y);
        bounds.min.z = std::min(bounds.min.z, p.z);
        bounds.max.x = std::max(bounds.max.x, p.x);
        bounds.max.y = std::max(bounds.max.y, p.y);
        bounds.max.z = std::max(bounds.max.z, p.z);
    }
    return bounds;
}

/// @brief 選択されたモデルをプレビュー用メッシュへ反映し（メッシュ・サブメッシュごとの本来のマテリアル）、
///        モデル全体が収まるようオービットカメラを自動フィットさせる
void ApplySelectedModelToPreview(SceneContext *sceneContext, ModelManager::ModelHandle handle) {
    auto &objects = GetModelPreviewObjects();
    auto &state = GetModelPreviewState();
    if (!sceneContext || !objects.valid) return;

    auto *meshObj = sceneContext->GetSceneObject(objects.meshObjectID);
    auto *meshFilter = meshObj ? meshObj->GetComponent<MeshFilter>() : nullptr;
    auto *meshRenderer = meshObj ? meshObj->GetComponent<MeshRenderer>() : nullptr;
    if (!meshFilter || !meshRenderer) return;

    meshFilter->SetMeshHandle(handle);
    state.appliedHandle = handle;

    if (handle == ModelManager::kInvalidHandle) {
        state.target = Vector3(0.0f, 0.0f, 0.0f);
        state.distance = 5.0f;
        return;
    }

    const ModelData &data = ModelManager::GetModelData(handle);

    // サブメッシュごとに、インポート時に自動生成された本来のマテリアルを割り当てる
    // （見つからない場合はMeshRendererの既定"Default"マテリアルのまま）
    const auto &subMeshes = data.GetSubMeshes();
    const size_t subMeshCount = std::max<size_t>(1, subMeshes.size());
    meshRenderer->SetMaterialSlotCount(subMeshCount);
    for (size_t i = 0; i < subMeshCount; ++i) {
        const std::uint32_t materialIndex = subMeshes.empty() ? 0u : subMeshes[i].materialIndex;
        const auto *material = data.GetMaterial(materialIndex);
        const std::string assetName = (material && !material->materialAssetName.empty())
            ? material->materialAssetName : std::string("Default");
        meshRenderer->SetMaterialNameAt(i, assetName);
    }

    // モデル全体が収まるようカメラの注視点・距離を自動フィットさせる
    const ModelBounds bounds = ComputeModelBounds(data);
    auto *cameraObj = sceneContext->GetSceneObject(objects.cameraObjectID);
    auto *camera3d = cameraObj ? cameraObj->GetComponent<Camera3D>() : nullptr;
    const float fovY = camera3d ? camera3d->GetFovY() : 0.45f;
    if (bounds.valid) {
        state.target = (bounds.min + bounds.max) * 0.5f;
        const float radius = std::max((bounds.max - bounds.min).Length() * 0.5f, 0.01f);
        const float halfFov = std::max(fovY * 0.5f, 0.05f);
        state.distance = radius / std::sin(halfFov) * 1.3f;
    } else {
        state.target = Vector3(0.0f, 0.0f, 0.0f);
        state.distance = 5.0f;
    }
    state.yaw = 0.6f;
    state.pitch = 0.35f;
}

/// @brief オービット状態からカメラのTransformを更新する（SceneEditorViewのシーンビューカメラと同じ計算式）
void UpdateModelPreviewCameraTransform(SceneContext *sceneContext) {
    const auto &objects = GetModelPreviewObjects();
    const auto &state = GetModelPreviewState();
    if (!sceneContext || !objects.valid) return;
    auto *cameraObj = sceneContext->GetSceneObject(objects.cameraObjectID);
    auto *transform = cameraObj ? cameraObj->GetComponent<Transform>() : nullptr;
    if (!transform) return;

    Matrix4x4 rotateX;
    rotateX.MakeRotateX(state.pitch);
    Matrix4x4 rotateY;
    rotateY.MakeRotateY(state.yaw);
    const Matrix4x4 rotation = rotateX * rotateY;
    const Vector3 forward(rotation.m[2][0], rotation.m[2][1], rotation.m[2][2]);
    const Vector3 eye = state.target - forward * state.distance;

    transform->SetTranslate(eye);
    transform->SetRotateQuaternion(Quaternion::MakeFromRotationMatrix(rotation));
}

/// @brief プレビュー画像上でのマウス操作（右ドラッグでオービット・ホイールでズーム）
void HandleModelPreviewCameraInput(ModelPreviewState &state) {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO &io = ImGui::GetIO();
    if (io.MouseWheel != 0.0f) {
        state.distance *= std::pow(0.9f, io.MouseWheel);
        state.distance = std::clamp(state.distance, 0.01f, 10000.0f);
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        state.yaw += io.MouseDelta.x * 0.005f;
        state.pitch += io.MouseDelta.y * 0.005f;
        state.pitch = std::clamp(state.pitch, -1.55f, 1.55f);
    }
}

/// @brief 選択中モデルの3Dプレビューを表示する（一覧テーブルの下に呼ぶ）
void ShowModelPreviewSection(SceneContext *sceneContext) {
    auto &state = GetModelPreviewState();

    if (state.selectedHandle == ModelManager::kInvalidHandle) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.select_hint"));
        return;
    }
    if (!sceneContext) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable"));
        return;
    }
    if (sceneContext->IsPlaying()) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable_playing"));
        DestroyModelPreviewObjects(sceneContext);
        return;
    }
    if (!EnsureModelPreviewObjects(sceneContext)) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable"));
        return;
    }
    if (state.appliedHandle != state.selectedHandle) {
        ApplySelectedModelToPreview(sceneContext, state.selectedHandle);
    }
    UpdateModelPreviewCameraTransform(sceneContext);

    if (ImGui::Button(TranslationLabel("editor.modelmanager.preview.reset_view"))) {
        ApplySelectedModelToPreview(sceneContext, state.selectedHandle);
        UpdateModelPreviewCameraTransform(sceneContext);
    }

    const auto &objects = GetModelPreviewObjects();
    auto *targetObj = sceneContext->GetSceneObject(objects.targetObjectID);
    auto *screenBufferObject = targetObj ? targetObj->GetComponent<ScreenBufferObject>() : nullptr;
    auto *screenBuffer = screenBufferObject ? screenBufferObject->GetScreenBuffer() : nullptr;
    if (!screenBuffer) {
        ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.unavailable"));
        return;
    }
    const auto srvHandle = screenBuffer->GetPreviewSrvHandle();
    if (srvHandle.ptr == 0) {
        ImGui::TextUnformatted(TranslationC("component.screenbufferobject.srv_not_ready"));
        return;
    }

    // アスペクト比を維持して表示領域にフィットさせる（ScreenBufferObject::ShowViewerWindowと同じ手法）
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float w = static_cast<float>(screenBuffer->GetWidth());
    const float h = static_cast<float>(screenBuffer->GetHeight());
    ImVec2 drawSize = avail;
    if (w > 0.0f && h > 0.0f && avail.x > 0.0f && avail.y > 0.0f) {
        const float scale = std::min(avail.x / w, avail.y / h);
        drawSize = ImVec2(w * scale, h * scale);
    }
    ImGui::Image(static_cast<ImTextureID>(srvHandle.ptr), drawSize);
    HandleModelPreviewCameraInput(state);
}

} // namespace

void ModelManager::ShowImGuiLoadedModelsWindow(SceneContext *sceneContext) {
    ImGui::Begin(TranslationLabel("editor.modelmanager.window"));

    const auto entries = ModelManager::GetImGuiModelListEntries();
    ImGui::Text(TranslationC("editor.modelmanager.loaded_models_d"), static_cast<int>(entries.size()));

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
            ImGui::PushID(static_cast<int>(e.handle));
            const bool isSelected = (GetModelPreviewState().selectedHandle == e.handle);
            if (ImGui::Selectable(std::to_string(e.handle).c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                GetModelPreviewState().selectedHandle = e.handle;
            }
            ImGui::PopID();

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

    ImGui::Separator();
    ImGui::TextUnformatted(TranslationC("editor.modelmanager.preview.header"));
    ShowModelPreviewSection(sceneContext);

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
