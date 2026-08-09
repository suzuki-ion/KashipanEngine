#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <DirectXTex.h>

#include "Utilities/Passkeys.h"
#include "Graphics/IShaderTexture.h"
#include "Utilities/Plugin/Texture/MipMapContainer.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class ShaderVariableBinder;
class ModelManager;

/// @brief テクスチャ管理クラス
class TextureManager final {
public:
    using TextureHandle = uint32_t;
    static constexpr TextureHandle kInvalidHandle = 0;

    class TextureView final : public IShaderTexture {
    public:
        TextureView() = default;
        explicit TextureView(TextureHandle h) : handle_(h) {}

        D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const noexcept override;
        std::uint32_t GetWidth() const noexcept override;
        std::uint32_t GetHeight() const noexcept override;

        TextureHandle GetHandle() const noexcept { return handle_; }

    private:
        TextureHandle handle_ = kInvalidHandle;
    };

    struct TextureListEntry final {
        TextureHandle handle = kInvalidHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t width = 0;
        uint32_t height = 0;
        uint64_t srvGpuPtr = 0;
        /// @brief キューブマップ（DDSのキューブマップフラグ由来）として読み込まれたかどうか。
        ///        TextureCubeRefのピッカーがTexture2D用の画像を誤って選べないようにする判定に使う
        bool isCubemap = false;
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

    /// @brief 外部管理のテクスチャ（ScreenBuffer / ShadowMapBuffer 等）を登録する
    /// @details SRVハンドルやサイズは毎回 texture から取得されるため、
    ///          ダブルバッファ等でSRVが入れ替わるテクスチャも通常テクスチャと同様に扱える。
    /// @param name 管理用の名前（ファイル名と同じ検索マップに登録される）
    /// @param texture 外部テクスチャ（登録解除まで生存していること）
    /// @return 登録したテクスチャのハンドル（失敗時は `kInvalidHandle`）
    static TextureHandle RegisterExternalTexture(const std::string& name, const IShaderTexture* texture);
    /// @brief 外部管理のテクスチャを登録解除する
    static bool UnregisterExternalTexture(TextureHandle handle);
    /// @brief 外部管理のテクスチャをポインタから登録解除する
    static bool UnregisterExternalTexture(const IShaderTexture* texture);

	/// @brief 指定ファイルパスのテクスチャを読み込む（Assets ルートからの相対 or フルパス）
	/// @return 読み込んだテクスチャの `ScratchImage`（失敗時は空の `ScratchImage`）
	DirectX::ScratchImage LoadTextureFromFile(const std::string& filePath);

    /// @brief メモリ上の画像データ（PNG/JPEG等、WICが認識できる形式）をデコードする
    /// @details glTF等、モデルファイル内部に埋め込まれたテクスチャ（bufferView参照）用
    /// @return デコードされたミップチェイン（失敗時は空の `ScratchImage`）
    DirectX::ScratchImage LoadTextureFromMemory(const void *data, size_t dataSize);

    /// @brief メモリ上の画像データからテクスチャを読み込み登録する
    /// @param registerPath 管理用の仮想パス（実ファイルは存在しない。例: "モデルのアセットパス:埋め込み画像名"）
    /// @return 登録したテクスチャのハンドル（失敗時は `kInvalidHandle`。同じregisterPathで登録済みの場合は既存のハンドルを返す）
    TextureHandle RegisterTextureFromMemory(const std::string &registerPath, const void *data, size_t dataSize);

    /// @brief 現在アクティブなTextureManagerインスタンスを取得する
    /// @details ModelManager等、他のManagerからのテクスチャ登録用。
    ///          エンジン実行中は常に唯一のTextureManagerが動いている前提。
    static TextureManager *GetActiveInstance(Passkey<ModelManager>);

    /// @brief ハンドルからテクスチャ(SRV index)を取得
    static TextureHandle GetTexture(TextureHandle handle);
    /// @brief ファイル名単体からテクスチャを取得
    static TextureHandle GetTextureFromFileName(const std::string& fileName);
    /// @brief Assetsルートからの相対パスからテクスチャを取得
    static TextureHandle GetTextureFromAssetPath(const std::string& assetPath);

    /// @brief ハンドルからテクスチャのファイル名を取得（無効時は空文字）
    static std::string GetTextureFileName(TextureHandle handle);
    /// @brief ハンドルからテクスチャのAssets相対パスを取得（無効時は空文字）
    static std::string GetTextureAssetPath(TextureHandle handle);

    /// @brief 読み込み済みテクスチャのファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイルを外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象テクスチャが見つかり更新に成功した場合は true
    static bool RenameTexture(const std::string &oldAssetPath, const std::string &newAssetPath);

    /// @brief 読み込み済みテクスチャ一覧を取得
    static std::vector<TextureListEntry> GetLoadedTextureListEntries();

    /// @brief ハンドルからシェーダー用テクスチャビューを取得（無効ハンドルの場合は空のビュー）
    static TextureView GetTextureView(TextureHandle handle) { return TextureView(GetTexture(handle)); }

    /// @brief シェーダーへテクスチャをバインドする（D3D12 のハンドルは外部へ出さない）
    static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, TextureHandle handle);

    /// @brief シェーダーへテクスチャをバインドする（IShaderTexture 経由）
    static bool BindTexture(ShaderVariableBinder* shaderBinder, const std::string& nameKey, const IShaderTexture& texture);

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれたテクスチャ一覧の ImGui ウィンドウを描画
    static void ShowImGuiLoadedTexturesWindow();

    /// @brief エディタのD&Dインポート等で、Assets以下に新規追加された1枚のテクスチャファイルを動的に読み込み登録する
    /// @param filePath Assets ルートからの相対パス（実ファイルが Assets 以下に存在している前提）
    /// @return 読み込んだテクスチャのハンドル（失敗時は `kInvalidHandle`）
    static TextureHandle LoadTextureDynamic(const std::string &filePath);
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

	Plugin::MipMapContainer mipMapContainer_;
};

} // namespace KashipanEngine
