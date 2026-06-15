#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "Math/Matrix4x4.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class ModelManager;

/// @brief モデルのデータ管理用構造体（Model 生成時のみ利用する想定）
struct ModelData final {
public:
    struct Vertex final {
        float px = 0.0f;
        float py = 0.0f;
        float pz = 0.0f;
        float nx = 0.0f;
        float ny = 0.0f;
        float nz = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        uint32_t boneIndices[4] = { 0, 0, 0, 0 };
        float boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };
    struct MaterialData final {
        float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::string diffuseTexturePath;
    };

    struct VertexWeightData final {
        float weight = 0.0f;
        uint32_t vertexIndex = 0;
    };
    struct JointWeightData final {
        Matrix4x4 inverseBindPoseMatrix;
        std::vector<VertexWeightData> vertexWeights;
    };

private:
    friend class ModelManager;
    friend class Model;

    ModelData() = default;

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // ジョイント名からスキンウェイトデータへのマップ
    std::unordered_map<std::string, JointWeightData> skinClusters_;
    std::vector<std::string> skinClusterNames_;

    std::vector<MaterialData> materials_;

    std::string assetRelativePath_;

public:
    uint32_t GetVertexCount() const noexcept { return static_cast<uint32_t>(vertices_.size()); }
    uint32_t GetIndexCount() const noexcept { return static_cast<uint32_t>(indices_.size()); }
    const std::vector<Vertex> &GetVertices() const noexcept { return vertices_; }
    const std::vector<uint32_t> &GetIndices() const noexcept { return indices_; }
    const std::unordered_map<std::string, JointWeightData> &GetSkinClusters() const noexcept { return skinClusters_; }
    const std::vector<std::string> &GetSkinClusterNames() const noexcept { return skinClusterNames_; }
    bool HasSkinning() const noexcept { return !skinClusters_.empty(); }
    const std::string &GetAssetRelativePath() const noexcept { return assetRelativePath_; }
    uint32_t GetMaterialCount() const noexcept { return static_cast<uint32_t>(materials_.size()); }
    const MaterialData *GetMaterial(uint32_t idx) const noexcept { return idx < materials_.size() ? &materials_[idx] : nullptr; }
};

/// @brief モデル管理クラス
class ModelManager final {
public:
    using ModelHandle = uint32_t;
    static constexpr ModelHandle kInvalidHandle = 0;

    struct ModelListEntry final {
        ModelHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param assetsRootPath Assets フォルダのルートパス
    ModelManager(Passkey<GameEngine>, const std::string& assetsRootPath = "Assets");
    ~ModelManager();

    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager(ModelManager&&) = delete;
    ModelManager& operator=(ModelManager&&) = delete;

    /// @brief 指定ファイルパスのモデルを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだモデルのハンドル（失敗時は `kInvalidHandle`）
    ModelHandle LoadModel(const std::string& filePath);

    /// @brief ファイル名単体からモデルハンドルを取得
    static ModelHandle GetModelHandleFromFileName(const std::string& fileName);
    /// @brief Assetsルートからの相対パスからモデルハンドルを取得
    static ModelHandle GetModelHandleFromAssetPath(const std::string& assetPath);

    /// @brief ハンドルからモデルデータを取得
    static const ModelData &GetModelData(ModelHandle handle);
    /// @brief ファイル名単体からモデルデータを取得
    static const ModelData &GetModelDataFromFileName(const std::string& fileName);
    /// @brief Assetsルートからの相対パスからモデルデータを取得
    static const ModelData &GetModelDataFromAssetPath(const std::string& assetPath);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれたモデル一覧を取得する
    static std::vector<ModelListEntry> GetLoadedModelListEntries();

    /// @brief デバッグ用: 読み込まれたモデル一覧の ImGui ウィンドウを描画
    static void ShowImGuiLoadedModelsWindow();
#endif

    const std::string& GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
#if defined(USE_IMGUI)
    static std::vector<ModelHandle> GetAllImGuiModels();
    static std::vector<ModelListEntry> GetImGuiModelListEntries();
#endif

    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    static inline const ModelData sEmptyData;
};

} // namespace KashipanEngine
