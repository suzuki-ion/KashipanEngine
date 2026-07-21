#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Assets/SamplerManager.h"
#include "Assets/TextureManager.h"
#include "Math/Matrix4x4.h"
#include "Math/Vector4.h"
#include "Utilities/FileIO.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;

/// @brief マテリアル管理クラス
class MaterialManager final {
public:
    using MaterialHandle = std::uint32_t;
    static constexpr MaterialHandle kInvalidHandle = 0;

    struct Material {
        std::string name = "Default";
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Matrix4x4 uvTransform = Matrix4x4::Identity();
        TextureManager::TextureHandle textureHandle = TextureManager::kInvalidHandle;
        TextureManager::TextureHandle environmentHandle = TextureManager::kInvalidHandle;
        SamplerManager::SamplerHandle samplerHandle = SamplerManager::kInvalidHandle;
        float shininess = 32.0f;
        Vector4 specularColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float environmentCoefficient = 1.0f;
        bool enableLighting = true;
        bool enableShadowMapProjection = true;
        /// @brief リムライト色（ライト方向を考慮した逆光縁取り）
        Vector4 rimColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        /// @brief リムの鋭さ（大きいほど縁が細くなる）
        float rimPower = 2.0f;
        /// @brief リムライトの強さ（0で無効。既定は無効のままにして既存マテリアルの見た目を変えない）
        float rimIntensity = 0.0f;

        /// @brief テクスチャファイル名（読み込み時に未解決だった場合の遅延解決用）
        std::string textureFileName;
        /// @brief 環境マップファイル名（読み込み時に未解決だった場合の遅延解決用）
        std::string environmentFileName;

        /// @brief 未解決のテクスチャハンドルをファイル名から解決する
        /// @details 読み込み時に対象テクスチャが存在しなかった場合でも、
        ///          ハンドルが得られるまで呼び出しの度に解決を試み続ける。
        void ResolveTextureHandles() {
            if (textureHandle == TextureManager::kInvalidHandle && !textureFileName.empty()) {
                textureHandle = TextureManager::GetTextureFromFileName(textureFileName);
            } else {
                // すでにハンドルが有効な場合はテクスチャが削除されていないか確認するために再取得する
                textureHandle = TextureManager::GetTexture(textureHandle);
            }
            if (environmentHandle == TextureManager::kInvalidHandle && !environmentFileName.empty()) {
                environmentHandle = TextureManager::GetTextureFromFileName(environmentFileName);
            } else {
                // すでにハンドルが有効な場合はテクスチャが削除されていないか確認するために再取得する
                environmentHandle = TextureManager::GetTexture(environmentHandle);
            }
        }
    };
    struct MaterialEntry final {
        std::string fullPath;
        std::string assetPath;
        std::string fileName;
        MaterialManager::Material material;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param assetsRootPath Assets フォルダのルートパス
    MaterialManager(Passkey<GameEngine>, const std::string &assetsRootPath = "Assets");
    ~MaterialManager();

    MaterialManager(const MaterialManager&) = delete;
    MaterialManager& operator=(const MaterialManager&) = delete;
    MaterialManager(MaterialManager&&) = delete;
    MaterialManager& operator=(MaterialManager&&) = delete;

    /// @brief 指定ファイルパスのマテリアルを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだマテリアルのハンドル（失敗時は `kInvalidHandle`）
    MaterialHandle LoadMaterial(const std::string& filePath);
    /// @brief 指定マテリアルを保存する
    /// @param handle 保存するマテリアルのハンドル
    /// @param filePath 保存先のファイルパス（空文字の場合は元のパスに上書き保存）
    /// @return 保存に成功した場合は true、失敗した場合は false
    static bool SaveMaterial(MaterialHandle handle, const std::string& filePath = "");
    /// @brief 全てのマテリアルを保存する
    /// @param folderPath 保存先のフォルダパス（空文字の場合は元のパスに上書き保存）
    /// @return 保存に成功した場合は true、失敗した場合は false
    static bool SaveAllMaterials(const std::string &folderPath = "");

    /// @brief ファイル名単体からマテリアルハンドルを取得
    /// @param fileName ファイル名（例: "my_material.mat"）
    static MaterialHandle GetMaterialHandleFromFileName(const std::string& fileName);
    /// @brief マテリアル名からハンドルを取得
    /// @param name マテリアル名（例: "MyMaterial"）
    static MaterialHandle GetMaterialHandleFromName(const std::string& name);

    /// @brief 読み込み済みマテリアルのファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイル（.mat）を外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。マテリアル自身の名前（Material::name、
    ///          sNameToHandleでの検索キー）は別概念のため変更しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象マテリアルが見つかり更新に成功した場合は true
    static bool RenameMaterialFile(const std::string& oldAssetPath, const std::string& newAssetPath);

    /// @brief ハンドルからマテリアルを取得
    static Material* GetMaterial(MaterialHandle handle);
    /// @brief マテリアル名からマテリアルを取得
    static Material* GetMaterial(const std::string& name);

    /// @brief マテリアルを登録
    static MaterialHandle RegisterMaterial(const std::string &name, const Material &material, const std::string &filePath = "");
    /// @brief マテリアルを削除
    static bool RemoveMaterial(const std::string& name);

    /// @brief 読み込み済みマテリアル一覧を取得
    static std::vector<MaterialEntry> GetLoadedMaterialListEntries();

#if defined(USE_IMGUI)
    /// @brief マテリアル管理用ImGuiウィンドウを描画（一覧・編集・追加・削除・保存）
    static void ShowImGuiMaterialManagerWindow();
    /// @brief 単一マテリアルの編集フィールド（色・テクスチャ・各種パラメータ）を描画する
    /// @details Assetsウィンドウの単一マテリアル編集ウィンドウ等、一覧テーブルを持たない
    ///          呼び出し元と共有するために切り出したもの。テクスチャ/環境マップの選択欄は
    ///          Assetsウィンドウからのドラッグ&ドロップも受け付ける。
    static void ShowMaterialEditorFields(Material &material);
#endif

    const std::string &GetAssetsRootPath() const noexcept;

private:
    void InitializeMaterialManager();
    void FinalizeMaterialManager();
    void LoadAllFromAssetsFolder();
};

} // namespace KashipanEngine
