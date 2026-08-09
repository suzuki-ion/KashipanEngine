#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;
class DirectXCommon;
class VideoPlayer;
class VideoTexture;
class Renderer;

/// @brief 動画アセット管理クラス
/// @details アセットの走査・メタデータ管理（VideoHandle）と、再生インスタンス(VideoPlayer)の
///          生成・毎フレーム更新を行う。実際のデコードはVideoPlayer::Play時まで遅延する
///          （ModelManagerと同様、起動時の走査では軽量なメタデータ取得のみ行う）。
class VideoManager final {
public:
    using VideoHandle = std::uint32_t;
    static constexpr VideoHandle kInvalidHandle = 0;

    struct VideoInfo final {
        std::string fullPath;
        std::string assetPath;
        std::string fileName;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        double durationSec = 0.0;
    };

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param directXCommon 動画テクスチャ生成に使う DirectXCommon
    /// @param assetsRootPath Assets フォルダのルートパス
    VideoManager(Passkey<GameEngine>, DirectXCommon *directXCommon, const std::string &assetsRootPath = "Assets");
    ~VideoManager();

    VideoManager(const VideoManager &) = delete;
    VideoManager &operator=(const VideoManager &) = delete;
    VideoManager(VideoManager &&) = delete;
    VideoManager &operator=(VideoManager &&) = delete;

    /// @brief 指定ファイルパスの動画を読み込む（メタデータのみ。実デコードはVideoPlayer::Play時に行う）
    /// @return 読み込んだ動画のハンドル（失敗時は `kInvalidHandle`）
    VideoHandle Load(const std::string &filePath);

    /// @brief ファイル名単体から動画ハンドルを取得
    static VideoHandle GetVideoHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスから動画ハンドルを取得
    static VideoHandle GetVideoHandleFromAssetPath(const std::string &assetPath);
    /// @brief 動画のメタデータを取得する（見つからない場合は nullptr）
    static const VideoInfo *GetVideoInfo(VideoHandle handle);

    /// @brief 読み込み済み動画のAssetsルートからの相対パス一覧を取得する（Asset選択UI用）
    static std::vector<std::string> GetLoadedVideoAssetPaths();

    /// @brief 読み込み済み動画のファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイルを外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象動画が見つかり更新に成功した場合は true
    static bool RenameVideo(const std::string &oldAssetPath, const std::string &newAssetPath);

    /// @brief 指定の動画を再生するVideoPlayerを新規生成する（所有権はVideoManagerが持つ）
    /// @return 生成したVideoPlayerのポインタ（失敗時は nullptr）
    static VideoPlayer *CreatePlayer(VideoHandle handle);
    /// @brief VideoPlayerを停止・破棄する
    /// @details 実体（音声停止・GPUリソース解放）の破棄は即座には行わず、CommitPendingDestroyまで
    ///          1フレーム遅延させる。ImGui::Image等が今フレーム中に既にSRVハンドルを描画コマンドへ
    ///          記録している可能性があり、GPUの描画完了より前に実体を破棄すると無効なSRV参照になるため
    ///          （ScreenBuffer::DestroyNotifyと同じ理由）
    static void DestroyPlayer(VideoPlayer *player);

    /// @brief DestroyPlayerで破棄予定になった全VideoPlayerの実体を破棄する
    /// @details GameLoopDraw完了（ImGuiの描画コマンド発行＋GPU同期）の後、GameEngine::Executeの
    ///          ループ末尾でScreenBuffer::CommitDestroy等と同じ並びで呼ぶこと
    static void CommitPendingDestroy(Passkey<GameEngine>);

    /// @brief 毎フレーム呼び出す更新処理（アクティブな全VideoPlayerのAV同期・GPUアップロードを行う）
    void Update();

    /// @brief YUV→RGB変換が必要なVideoTexture一覧を取得する（Renderer::ProcessVideoConversions専用）
    static std::vector<VideoTexture *> GetPendingConversions(Passkey<Renderer>);

    const std::string &GetAssetsRootPath() const noexcept { return assetsRootPath_; }

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれた動画一覧のImGuiウィンドウを描画
    static void ShowImGuiLoadedVideosWindow();
    /// @brief デバッグ用: 再生中の動画一覧のImGuiウィンドウを描画
    static void ShowImGuiPlayingVideosWindow();

    /// @brief エディタのD&Dインポート等で、Assets以下に新規追加された1つの動画ファイルを動的に読み込み登録する
    /// @param filePath Assets ルートからの相対パス（実ファイルが Assets 以下に存在している前提）
    /// @return 読み込んだ動画のハンドル（失敗時は `kInvalidHandle`）
    static VideoHandle LoadDynamic(const std::string &filePath);
#endif

private:
    void InitializeMediaFoundation();
    void FinalizeMediaFoundation();
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
    DirectXCommon *directXCommon_ = nullptr;
    bool mfInitialized_ = false;
};

} // namespace KashipanEngine
