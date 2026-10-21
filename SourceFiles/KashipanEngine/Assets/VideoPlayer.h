#pragma once
#include <cstdint>
#include <memory>
#include <string>

#include "Assets/AudioManager.h"
#include "Assets/MaterialManager.h"
#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class DirectXCommon;
class VideoManager;
class VideoTexture;

/// @brief 動画の1再生インスタンス
/// @details 専用デコードスレッドで IMFSourceReader から映像フレーム(NV12)をストリーミングデコードし、
///          音声トラックは再生開始時に全体PCMデコードして AudioManager へ登録・再生する。
///          AudioManager::GetPlayPositionSeconds をマスタークロックにフレーム表示タイミングを制御する。
///          Media Foundation / XAudio2 / スレッド関連の実装詳細はすべて .cpp 側の Impl 構造体に隠蔽している。
class VideoPlayer final {
public:
    /// @param directXCommon GPUテクスチャ生成に使う DirectXCommon
    /// @param fullPath 動画ファイルのフルパス
    /// @param registerNamePrefix TextureManager/AudioManagerへ登録する際の一意な名前の接頭辞
    VideoPlayer(DirectXCommon *directXCommon, std::string fullPath, std::string registerNamePrefix);
    ~VideoPlayer();

    VideoPlayer(const VideoPlayer &) = delete;
    VideoPlayer &operator=(const VideoPlayer &) = delete;
    VideoPlayer(VideoPlayer &&) = delete;
    VideoPlayer &operator=(VideoPlayer &&) = delete;

    /// @brief 再生を開始する
    /// @param loop ループ再生するかどうか
    /// @param volume 音量(0.0～1.0)
    /// @return 成功した場合 true
    bool Play(bool loop = false, float volume = 1.0f);
    /// @brief 動画の最初の1フレームだけを同期的にデコードし、静止画として表示できるようにする
    /// @details 音声デコードやデコードスレッドの起動は行わず、IsPlaying/IsPausedはfalseのまま。
    ///          再生停止中（ゲームループ開始前のプレビュー等）に、動画の1フレーム目を
    ///          プレースホルダーとして表示し続けたい場合に使う。既に再生中の場合は失敗する
    /// @return 成功した場合 true
    bool ShowFirstFrame();
    /// @brief 再生を停止する（デコードスレッドを終了し、音声再生も止める。登録したテクスチャ/音声も解放する）
    void Stop();
    /// @brief 一時停止する
    void Pause();
    /// @brief 一時停止を解除する
    void Resume();

    bool IsPlaying() const noexcept;
    bool IsPaused() const noexcept;

    /// @brief 毎フレーム呼び出す更新処理（VideoManager::Update から呼ばれる）
    /// @details 音声の再生位置を基準に、表示すべきフレームをGPUへアップロードする
    void Update();

    /// @brief 対象マテリアルのtextureHandleを、この動画のYUV→RGB変換後テクスチャで上書きする
    /// @details 変換後は通常のRGBAテクスチャとして扱われるため、既存のどのパイプライン（2D/3D、
    ///          ライティング有無を問わず）を使っているマテリアルでもそのまま適用できる。
    ///          再生開始(Play成功)後に呼び出す必要がある
    void ConfigureMaterial(MaterialManager::MaterialHandle materialHandle) const;

    std::uint32_t GetWidth() const noexcept;
    std::uint32_t GetHeight() const noexcept;
    const std::string &GetFullPath() const noexcept { return fullPath_; }

    /// @brief YUV→RGB変換後テクスチャのTextureManagerハンドルを取得する（プレビュー表示等に使う）
    TextureManager::TextureHandle GetTextureHandle() const noexcept;

    /// @brief 内部のVideoTextureを取得する（Renderer::ProcessVideoConversions向けにVideoManagerが中継する）
    VideoTexture *GetVideoTexture(Passkey<VideoManager>) const;

    /// @brief 音声トラック再生に使っているAudioManagerのPlayHandleを取得する
    /// @details 映像フレーム表示タイミングのマスタークロックとして使われているハンドルそのものなので、
    ///          呼び出し側でAudioManager::Stop()等を呼んで停止させないこと（VideoPlayer::Stop()が
    ///          自身の責任で停止・解放する）。AudioSource::AttachExternalPlayHandle()経由で
    ///          Spatial Audio/エフェクトだけを重ねて適用する用途を想定している
    AudioManager::PlayHandle GetAudioPlayHandle() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string fullPath_;
};

} // namespace KashipanEngine
