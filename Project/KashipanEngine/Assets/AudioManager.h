#pragma once

#include <cstdint>
#include <string>
#include <vector>

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
