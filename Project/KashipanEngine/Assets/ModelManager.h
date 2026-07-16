#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "Math/Matrix4x4.h"
#include "Math/Vector3.h"
#include "Utilities/Passkeys.h"

struct aiScene;
struct aiNode;
struct aiMesh;

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

    /// @brief BlendShape（モーフターゲット）の頂点1つ分の差分（バインドポーズからの差分のみ保持する）
    struct BlendShapeVertexDelta final {
        Vector3 deltaPosition{ 0.0f, 0.0f, 0.0f };
        Vector3 deltaNormal{ 0.0f, 0.0f, 0.0f };
        uint32_t vertexIndex = 0;
    };
    /// @brief BlendShape1つ分のデータ（変化の無い頂点は保持しないスパース表現）
    struct BlendShapeData final {
        std::string name;
        std::vector<BlendShapeVertexDelta> deltas;
    };

private:
    friend class ModelManager;
    friend class Model;

    ModelData() = default;

    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;

    // ジョイント名からスキンウェイトデータへのマップ
    std::unordered_map<std::string, JointWeightData> skinClusters_;

    // BlendShape（モーフターゲット）一覧
    std::vector<BlendShapeData> blendShapes_;

    std::vector<MaterialData> materials_;

    std::string assetRelativePath_;

public:
    uint32_t GetVertexCount() const noexcept { return static_cast<uint32_t>(vertices_.size()); }
    uint32_t GetIndexCount() const noexcept { return static_cast<uint32_t>(indices_.size()); }
    const std::vector<Vertex> &GetVertices() const noexcept { return vertices_; }
    const std::vector<uint32_t> &GetIndices() const noexcept { return indices_; }
    const std::string &GetAssetRelativePath() const noexcept { return assetRelativePath_; }
    uint32_t GetMaterialCount() const noexcept { return static_cast<uint32_t>(materials_.size()); }
    const MaterialData *GetMaterial(uint32_t idx) const noexcept { return idx < materials_.size() ? &materials_[idx] : nullptr; }

    /// @brief ジョイント名からスキンウェイトデータへのマップを取得（SkinnedMeshRenderer用）
    const std::unordered_map<std::string, JointWeightData> &GetSkinClusters() const noexcept { return skinClusters_; }
    /// @brief スキニング用のジョイントウェイトデータを持つか（ボーンが1つも無いメッシュはfalse）
    bool HasSkinning() const noexcept { return !skinClusters_.empty(); }

    /// @brief BlendShape（モーフターゲット）一覧を取得（SkinnedMeshRenderer用）
    const std::vector<BlendShapeData> &GetBlendShapes() const noexcept { return blendShapes_; }
    /// @brief BlendShapeを1つ以上持つか
    bool HasBlendShapes() const noexcept { return !blendShapes_.empty(); }
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

    /// @brief 頂点・インデックス配列からメッシュを登録する（プリミティブメッシュ等の動的生成用）
    /// @details 実ファイルを伴わないメッシュを登録する。同名で登録済みの場合は既存のハンドルを返す。
    /// @param name 登録名（重複防止のためファイル名・アセットパスの両方として使われる）
    /// @return 登録したメッシュのハンドル（失敗時は `kInvalidHandle`）
    static ModelHandle RegisterProceduralMesh(const std::string& name, std::vector<ModelData::Vertex> vertices, std::vector<std::uint32_t> indices);

    /// @brief ファイル名単体からモデルハンドルを取得
    static ModelHandle GetModelHandleFromFileName(const std::string& fileName);
    /// @brief Assetsルートからの相対パスからモデルハンドルを取得
    static ModelHandle GetModelHandleFromAssetPath(const std::string& assetPath);

    /// @brief サブメッシュのアセットパス（"モデルパス:ノード名"）から元のモデルファイルのアセットパスを取得する
    /// @details 複数のメッシュノードを持つモデルは、ノード単位のサブメッシュが "モデルパス:ノード名" の
    ///          形式で個別登録される。スケルトンやアニメーションはモデルファイル単位で管理されるため、
    ///          それらの解決にはこの関数で得た元のパスを使うこと。
    ///          区切り文字が含まれない通常のパスはそのまま返す
    static std::string GetBaseAssetPath(const std::string& assetPath);

    /// @brief 読み込み済みモデルのファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイルを外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象モデルが見つかり更新に成功した場合は true
    static bool RenameModel(const std::string& oldAssetPath, const std::string& newAssetPath);

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

    /// @brief Assimpのメッシュ1つ分（頂点・インデックス・スキンウェイト・BlendShape）をModelDataへ追記する
    static void AppendMeshToModelData(const aiMesh *mesh, ModelData &dst);
    /// @brief ノード分解によるサブメッシュ登録と、モデル階層のプレハブ（.prefab）自動生成を行う
    /// @details メッシュを持つノードが複数ある場合、各ノードのメッシュを "モデルパス:ノード名" の
    ///          アセットパスで個別登録する（Unityのモデル内サブアセットに相当）。
    ///          さらにエディタービルドでは、モデルのノード階層（アーマチュア・メッシュオブジェクト）を
    ///          そのまま再現するプレハブファイルをモデルの隣（"モデルファイル名.prefab"）へ生成する
    ///          （既にプレハブが存在する場合はユーザーの編集を保護するため上書きしない）
    void RegisterNodeDecompositionAndPrefab(const aiScene *scene,
        const std::string &modelFullPath, const std::string &baseAssetPath,
        const std::string &baseFileName, const std::vector<ModelData::MaterialData> &materials);

    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    static inline const ModelData sEmptyData;
};

} // namespace KashipanEngine
