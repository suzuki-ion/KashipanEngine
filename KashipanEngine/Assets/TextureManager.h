#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class ShaderVariableBinder;

/// @brief テクスチャ管理クラス
class TextureManager final {
public:
    using TextureHandle = uint32_t;
    static constexpr TextureHandle kInvalidHandle = 0;

    struct TextureListEntry final {
        TextureHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t srvGpuPtr = 0;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param directXCommon DirectX 共通（device / SRV heap 取得用）
    /// @param assetsRootPath Assets フォルダのルートパス
    TextureManager(Passkey<GameEngine>, DirectXCommon* directXCommon, const std::string& assetsRootPath = "Assets");
    ~TextureManager();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    /// @brief 指定ファイルパスのテクスチャを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだテクスチャのハンドル（失敗時は `kInvalidHandle`）
    TextureHandle LoadTexture(const std::string& filePath);

    /// @brief ハンドルからテクスチャ(SRV index)を取得
    static TextureHandle GetTexture(TextureHandle handle);
    /// @brief ファイル名単体からテクスチャを取得
    static TextureHandle GetTextureFromFileName(const std::string& fileName);
    /// @brief Assetsルートからの相対パスからテクスチャを取得
    static TextureHandle GetTextureFromAssetPath(const std::string& assetPath);

    /// @brief シェーダーへテクスチャをバインドする（D3D12 のハンドルは外部へ出さない）
    static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, TextureHandle handle);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれたテクスチャ一覧の ImGui ウィンドウを描画
    static void ShowImGuiLoadedTexturesWindow();
#endif

    const std::string& GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
#if defined(USE_IMGUI)
    static std::vector<TextureHandle> GetAllImGuiTextures();
    static std::vector<TextureListEntry> GetImGuiTextureListEntries();
#endif
    void LoadAllFromAssetsFolder();

    DirectXCommon* directXCommon_ = nullptr;
    std::string assetsRootPath_;
};

} // namespace KashipanEngine
