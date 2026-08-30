#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class GifTexture;
class GifPlayer;

/// @brief GIFアニメーション管理クラス
/// @details Assetsフォルダの.gifを起動時に全フレームデコードしてキャッシュする。
///          動画(VideoManager)と異なりデコードは軽量（ストリーミング・音声が無い）ため、
///          メタデータのみの先行走査＋再生時の遅延デコードは行わず、TextureManagerの
///          画像読み込みと同様にスレッドプールで即座に全フレームをデコードする。
///          再生の進行（フレーム切り替えタイミング）はGifManager自身ではなくGifSource側が行うため、
///          毎フレームのUpdate()呼び出しは不要（VideoManagerとの一番の違い）。
class GifManager final {
public:
    using GifHandle = std::uint32_t;
    static constexpr GifHandle kInvalidHandle = 0;

    struct GifFrame final {
        /// @brief このフレーム時点のキャンバス全体のRGBA8ピクセル列（width*height*4バイト）
        std::vector<std::uint8_t> rgba;
        /// @brief このフレームを表示し続ける秒数
        float delaySeconds = 0.1f;
    };

    struct GifAnimation final {
        std::string fullPath;
        std::string assetPath;
        std::string fileName;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::vector<GifFrame> frames;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param directXCommon 再生用GifTexture生成に使うDirectXCommon
    /// @param assetsRootPath Assets フォルダのルートパス
    GifManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath = "Assets");
    ~GifManager();

    GifManager(const GifManager &) = delete;
    GifManager &operator=(const GifManager &) = delete;
    GifManager(GifManager &&) = delete;
    GifManager &operator=(GifManager &&) = delete;

    /// @brief 指定ファイルパスのGIFを読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだGIFのハンドル（失敗時は `kInvalidHandle`）
    GifHandle Load(const std::string &filePath);

    /// @brief ファイル名単体からGIFハンドルを取得
    static GifHandle GetGifHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスからGIFハンドルを取得
    static GifHandle GetGifHandleFromAssetPath(const std::string &assetPath);
    /// @brief GIFのデコード済みアニメーションデータを取得する（見つからない場合は nullptr）
    static const GifAnimation *GetGifAnimation(GifHandle handle);

    /// @brief 読み込み済みGIFのAssetsルートからの相対パス一覧を取得する（Asset選択UI用）
    static std::vector<std::string> GetLoadedGifAssetPaths();

    /// @brief 読み込み済みGIFのファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイルを外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象GIFが見つかり更新に成功した場合は true
    static bool RenameGif(const std::string &oldAssetPath, const std::string &newAssetPath);

    /// @brief 指定GIFを再生するGifPlayerを新規生成する（所有権は呼び出し元が持つ）
    /// @details GifSourceコンポーネント・Assetsウィンドウのプレビューウィンドウ等、
    ///          GIFを表示したい側はこれ経由でGifPlayerを取得し、フレーム送り・テクスチャ
    ///          ライフサイクル管理を任せる（自前でGifTextureを扱わないこと）
    /// @return 生成したGifPlayer（失敗時は nullptr）
    static std::unique_ptr<GifPlayer> CreatePlayer(GifHandle handle);

    /// @brief GifPlayerを破棄する
    /// @details 実体（GifTextureのGPUリソース・SRV）の破棄は即座には行わず、CommitPendingDestroyまで
    ///          1フレーム遅延させる。ImGui::Image等が今フレーム中に既にSRVハンドルを描画コマンドへ
    ///          記録している可能性があり、GPUの描画完了より前に実体を破棄すると無効なSRV参照になるため
    ///          （ScreenBuffer::DestroyNotify/VideoManager::DestroyPlayerと同じ理由）。
    ///          呼び出し元は`player`（自分が持っていたunique_ptr）をムーブして渡すこと
    static void DestroyPlayer(std::unique_ptr<GifPlayer> player);

    /// @brief DestroyPlayerで破棄予定になった全GifPlayerの実体を破棄する
    /// @details GameLoopDraw完了（ImGuiの描画コマンド発行＋GPU同期）の後、GameEngine::Executeの
    ///          ループ末尾でVideoManager::CommitPendingDestroy等と同じ並びで呼ぶこと
    static void CommitPendingDestroy(Passkey<GameEngine>);

    /// @brief 指定GIFの現在フレームを表示するための専用GifTextureを生成する
    /// @details GifPlayer専用。所有権は呼び出し元（GifPlayer）が持つ
    /// @return 生成したGifTexture（失敗時は nullptr）
    static std::unique_ptr<GifTexture> CreateGifTexture(Passkey<GifPlayer>, GifHandle handle);

#if defined(USE_IMGUI)
    /// @brief エディタのD&Dインポート等で、Assets以下に新規追加された1つのGIFファイルを動的に読み込み登録する
    /// @param filePath Assets ルートからの相対パス（実ファイルが Assets 以下に存在している前提）
    /// @return 読み込んだGIFのハンドル（失敗時は `kInvalidHandle`）
    static GifHandle LoadDynamic(const std::string &filePath);
#endif

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

private:
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    DirectXCommon *directXCommon_ = nullptr;
};

} // namespace KashipanEngine
