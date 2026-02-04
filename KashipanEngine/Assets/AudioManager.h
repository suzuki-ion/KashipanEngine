#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <limits>
#include <chrono>

#include "Utilities/Passkeys.h"

namespace KashipanEngine {

class GameEngine;

/// @brief 音声管理クラス
class AudioManager final {
public:
    using SoundHandle = uint32_t;
    static constexpr SoundHandle kInvalidSoundHandle = 0;

    using PlayHandle = uint32_t;
    static constexpr PlayHandle kInvalidPlayHandle = 0;

    /// @brief コンストラクタ（GameEngine からのみ生成可能）
    /// @param assetsRootPath Assets フォルダのルートパス
    AudioManager(Passkey<GameEngine>, const std::string& assetsRootPath = "Assets");
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    AudioManager(AudioManager&&) = delete;
    AudioManager& operator=(AudioManager&&) = delete;

    /// @brief 指定ファイルパスの音声を読み込む（Assets ルートからの相対 or フルパス）
    /// @return 読み込んだ音声のハンドル（失敗時は `kInvalidSoundHandle`）
    SoundHandle Load(const std::string& filePath);

    /// @brief ファイル名単体から音声ハンドルを取得
    static SoundHandle GetSoundHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスから音声ハンドルを取得
    static SoundHandle GetSoundHandleFromAssetPath(const std::string &assetPath);

    /// @brief 音声を再生する
    /// @param sound 再生する音声ハンドル
    /// @param volume ボリューム (0.0f ~ 1.0f)
    /// @param pitch ピッチ（半音単位。+1.0f で半音上がる）
    /// @param loop ループ再生
    /// @return 再生ハンドル（失敗時は `kInvalidPlayHandle`）
    static PlayHandle Play(SoundHandle sound, float volume = 1.0f, float pitch = 0.0f, bool loop = false);

    /// @brief 再生停止
    /// @return 成功した場合 true
    static bool Stop(PlayHandle play);

    /// @brief 一時停止
    /// @return 成功した場合 true
    static bool Pause(PlayHandle play);

    /// @brief 一時停止解除
    /// @return 成功した場合 true
    static bool Resume(PlayHandle play);

    /// @brief 再生中の音量を設定する
    /// @param play 再生ハンドル
    /// @param volume ボリューム (0.0f ~ 1.0f)
    /// @return 成功した場合 true
    static bool SetVolume(PlayHandle play, float volume);

    /// @brief 再生中のピッチを設定する（半音単位）
    /// @param play 再生ハンドル
    /// @param pitch ピッチ（半音単位。+1.0f で半音上がる）
    /// @return 成功した場合 true
    static bool SetPitch(PlayHandle play, float pitch);

    /// @brief 再生中かどうか
    static bool IsPlaying(PlayHandle play);

    /// @brief 一時停止中かどうか
    static bool IsPaused(PlayHandle play);

    /// @brief 更新処理（再生状態の監視など）
    void Update();

#if defined(USE_IMGUI)
    /// @brief デバッグ用: 読み込まれた音声一覧の ImGui ウィンドウを描画
    static void ShowImGuiLoadedSoundsWindow();
    /// @brief デバッグ用: 再生中/保持中の音声一覧の ImGui ウィンドウを描画
    static void ShowImGuiPlayingSoundsWindow();
#endif

    const std::string& GetAssetsRootPath() const noexcept { return assetsRootPath_; }

    class SoundBeat {
    public:
        SoundBeat();
        /// @brief コンストラクタ
        /// @param play 再生中の音声のプレイハンドル
        /// @param bpm BPM値（例：120.0f）
        /// @param startOffsetSec 音声開始からビート開始までのオフセット時間（秒）
        SoundBeat(PlayHandle play, float bpm, double startOffsetSec);
        ~SoundBeat();

        /// @brief 現在の再生位置をポーリングしてビート管理を行う更新関数（AudioManager から自動で呼ばれる）
        void Update(Passkey<AudioManager>);

        /// @brief ビート情報の設定
        /// @param play 再生中の音声のプレイハンドル
        /// @param bpm BPM値（例：120.0f）
        /// @param startOffsetSec 音声開始からビート開始までのオフセット時間（秒）
        void SetBeat(PlayHandle play, float bpm, double startOffsetSec);

        /// @brief 再生ハンドルの設定
        /// @param play 再生中の音声のプレイハンドル
        void SetPlayHandle(PlayHandle play) noexcept;

        /// @brief BPM値の設定
        /// @param bpm BPM値（例：120.0f）
        void SetBPM(float bpm) noexcept { bpm_ = bpm; }

        /// @brief 開始オフセット時間の設定
        /// @param startOffsetSec 音声開始からビート開始までのオフセット時間（秒）
        void SetStartOffsetSec(double startOffsetSec) noexcept { startOffsetSec_ = startOffsetSec; }

        /// @brief ビート到達時のコールバック設定
        /// @param cb コールバック関数（引数：再生ハンドル、拍インデックス、拍到達時の再生位置秒）
        void SetOnBeat(std::function<void(PlayHandle, uint64_t, double)> cb);

        /// @brief 拍の進行状況取得用関数
        /// @return 拍の進行状況（0.0f ～ 1.0f）
        float GetBeatProgress() const;

        /// @brief 現在の拍インデックス取得用関数
        /// @return 現在の拍インデックス（0 から始まる連番。未開始時は uint64_t の最大値）
        uint64_t GetCurrentBeat() const noexcept { return currentBeatIndex_; }

        /// @brief 現在の拍インデックスをリセット
        void Reset();

        /// @brief アクティブかどうか
        bool IsActive() const noexcept { return bpm_ > 0.0f; }

        /// @brief ビート到達がトリガーされたかどうか
        bool IsOnBeatTriggered() const noexcept { return isOnBeatTriggered_; }

        /// @brief 拍の判定を手動時間で開始する（再生ハンドルが無い場合のみ）
        void StartManualBeat();

        /// @brief 拍の判定を停止（再生ハンドルが無い場合のみ）
        void StopManualBeat() {
            isUseManualTime_ = false;
        }

    private:
        PlayHandle playHandle_{ kInvalidPlayHandle };
        float bpm_{ 0.0f };
        double startOffsetSec_{ 0.0 }; // 秒
        bool isOnBeatTriggered_{ false };

        uint64_t currentBeatIndex_{ std::numeric_limits<uint64_t>::max() };

        std::function<void(PlayHandle, uint64_t, double)> onBeatCallback_;

        bool isUseManualTime_{ false };
        std::chrono::steady_clock::time_point manualStartTime_{};
    };

    /// @brief 再生中の音声の現在位置を秒単位で取得する
    /// @param play 再生ハンドル
    /// @param outSeconds 取得した秒数の出力先
    /// @return 成功した場合 true
    static bool GetPlayPositionSeconds(PlayHandle play, double& outSeconds);

private:
#if defined(USE_IMGUI)
    struct SoundListEntry final {
        SoundHandle handle = kInvalidSoundHandle;
        std::string fileName;
        std::string assetPath;
        uint32_t channels = 0;
        uint32_t samplesPerSec = 0;
        uint32_t bitsPerSample = 0;
        uint32_t durationMs = 0;
    };

    struct PlayingListEntry final {
        PlayHandle playHandle = kInvalidPlayHandle;
        SoundHandle soundHandle = kInvalidSoundHandle;
        std::string fileName;
        std::string assetPath;
        bool isPlaying = false;
        bool isPaused = false;
    };

    static std::vector<SoundListEntry> GetImGuiSoundListEntries();
    static std::vector<PlayingListEntry> GetImGuiPlayingListEntries();
#endif

    void InitializeAudioDevice();
    void FinalizeAudioDevice();
    void LoadAllFromAssetsFolder();

    std::string assetsRootPath_;
};

} // namespace KashipanEngine
