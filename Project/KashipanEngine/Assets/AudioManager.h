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
class AudioPlayer;
class SoundBeat;

/// @brief 音声管理クラス
class AudioManager final {
public:
    using SoundHandle = uint32_t;
    static constexpr SoundHandle kInvalidSoundHandle = 0;

    using PlayHandle = uint32_t;
    static constexpr PlayHandle kInvalidPlayHandle = 0;

    /// @brief 再生中の音声にかけるフィルター種別
    enum class FilterType {
        LowPass,
        HighPass,
        BandPass,
        Notch,
    };

    /// @brief 4バンドイコライザーのパラメータ（FXEQ準拠）
    struct EqParams final {
        /// @brief 各バンドの中心周波数(Hz、20～20000)
        float frequencyCenter[4] = { 100.0f, 800.0f, 2000.0f, 10000.0f };
        /// @brief 各バンドの増幅率（1.0で等倍、0.126～7.94）
        float gain[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        /// @brief 各バンドの帯域幅（中心周波数を基準としたオクターブ幅、0.1～2.0）
        float bandwidth[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    /// @brief エコーのパラメータ（FXECHO準拠）
    struct EchoParams final {
        /// @brief 原音とエコー音の混合比（0.0:原音のみ ～ 1.0:エコー音のみ）
        float wetDryMix = 0.5f;
        /// @brief エコーの繰り返し量（0.0:繰り返し無し ～ 1.0:減衰しない）
        float feedback = 0.5f;
        /// @brief 遅延時間(ミリ秒、1～2000)
        float delayMs = 500.0f;
    };

    /// @brief マスタリングリミッターのパラメータ（FXMASTERINGLIMITER準拠）
    struct LimiterParams final {
        /// @brief 圧縮を解除するまでの時間（サンプル数基準の係数、1～20）
        uint32_t release = 6;
        /// @brief 音量の閾値（値が小さいほど強く圧縮される、1～1800）
        uint32_t loudness = 1000;
    };

    struct PlayParams final {
        SoundHandle sound = kInvalidSoundHandle;
        float volume = 1.0f;
        float pitch = 0.0f;
        bool loop = false;
        double startTimeSec = 0.0;
        double endTimeSec = 0.0;
    };

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

    /// @brief デコード済みのPCM(整数リニアPCM)データを直接音声として登録する（ファイルスキャンを経由しない）
    /// @details 動画ファイルの音声トラックを抽出して登録する用途などに使う。
    ///          `registerName` はファイル名/Assetsパス双方の代わりとして使われるため、
    ///          他の登録済み音声と重複しない一意な名前を渡すこと。
    /// @param registerName 登録名（ファイル名/アセットパスとして扱われる）
    /// @param channels チャンネル数
    /// @param samplesPerSec サンプリングレート(Hz)
    /// @param bitsPerSample 1サンプルあたりビット数（通常16）
    /// @param pcmData インターリーブ済みのPCMサンプルバイト列
    /// @return 登録した音声のハンドル（失敗時は `kInvalidSoundHandle`）
    static SoundHandle RegisterSoundFromMemory(const std::string &registerName,
        uint32_t channels, uint32_t samplesPerSec, uint32_t bitsPerSample,
        const std::vector<uint8_t> &pcmData);

    /// @brief ファイル名単体から音声ハンドルを取得
    static SoundHandle GetSoundHandleFromFileName(const std::string &fileName);
    /// @brief Assetsルートからの相対パスから音声ハンドルを取得
    static SoundHandle GetSoundHandleFromAssetPath(const std::string &assetPath);

    /// @brief 読み込み済み音声のAssetsルートからの相対パス一覧を取得する（Asset選択UI用）
    static std::vector<std::string> GetLoadedSoundAssetPaths();

    /// @brief 読み込み済み音声のファイル名/パス登録をリネーム後の値へ更新する
    /// @details 実ファイルを外部（Assetsウィンドウ等）でリネーム/移動した後に呼ぶこと。
    ///          このメソッド自体はファイルの実体は操作しない。
    /// @param oldAssetPath リネーム前のAssetsルートからの相対パス
    /// @param newAssetPath リネーム後のAssetsルートからの相対パス
    /// @return 対象音声が見つかり更新に成功した場合は true
    static bool RenameSound(const std::string &oldAssetPath, const std::string &newAssetPath);

    /// @brief 音声を再生する
    /// @param sound 再生する音声ハンドル
    /// @param volume ボリューム (0.0f ~ 1.0f)
    /// @param pitch ピッチ（半音単位。+1.0f で半音上がる）
    /// @param loop ループ再生
    /// @param startTimeSec 再生開始時間（秒）
    /// @param endTimeSec 再生終了時間（秒。0以下は末尾まで）
    /// @return 再生ハンドル（失敗時は `kInvalidPlayHandle`）
    static PlayHandle Play(SoundHandle sound, float volume = 1.0f, float pitch = 0.0f,
        bool loop = false, double startTimeSec = 0.0, double endTimeSec = 0.0);

    /// @brief 再生パラメータをまとめた構造体から再生する
    /// @param params 再生パラメータ
    /// @return 再生ハンドル（失敗時は `kInvalidPlayHandle`）
    static PlayHandle Play(const PlayParams& params);

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

    /// @brief 再生中の定位（パン）を設定する
    /// @param play 再生ハンドル
    /// @param pan パン (-1.0f: 左 ～ 0.0f: 中央 ～ 1.0f: 右)
    /// @return 成功した場合 true
    static bool SetPan(PlayHandle play, float pan);

    /// @brief 再生中の音声にかけるフィルターを設定する
    /// @param play 再生ハンドル
    /// @param type フィルター種別
    /// @param frequency 正規化カットオフ周波数 (0.0f に近いほど強くかかる、最大 1.0f でフィルター無しと同等)
    /// @param oneOverQ フィルターの効き具合（共鳴）。既定値は 1.0f
    /// @return 成功した場合 true
    static bool SetFilter(PlayHandle play, FilterType type, float frequency, float oneOverQ = 1.0f);

    /// @brief 再生中の音声にかけるリバーブの送り量(ウェットレベル)を設定する
    /// @param play 再生ハンドル
    /// @param wetLevel リバーブの送り量 (0.0f: リバーブ無し ～ 1.0f: 最大)
    /// @return 成功した場合 true（リバーブ初期化に失敗している場合は false）
    static bool SetReverbSend(PlayHandle play, float wetLevel);

    /// @brief 再生中の音声にかけるイコライザーの有効/無効とパラメータを設定する
    /// @return 成功した場合 true（エフェクトチェーンの初期化に失敗している場合は false）
    static bool SetEqEffect(PlayHandle play, bool enabled, const EqParams &params);

    /// @brief 再生中の音声にかけるエコーの有効/無効とパラメータを設定する
    /// @return 成功した場合 true（エフェクトチェーンの初期化に失敗している場合は false）
    static bool SetEchoEffect(PlayHandle play, bool enabled, const EchoParams &params);

    /// @brief 再生中の音声にかけるマスタリングリミッターの有効/無効とパラメータを設定する
    /// @return 成功した場合 true（エフェクトチェーンの初期化に失敗している場合は false）
    static bool SetLimiterEffect(PlayHandle play, bool enabled, const LimiterParams &params);

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

    /// @brief エディタのD&Dインポート等で、Assets以下に新規追加された1つの音声ファイルを動的に読み込み登録する
    /// @param filePath Assets ルートからの相対パス（実ファイルが Assets 以下に存在している前提）
    /// @return 読み込んだ音声のハンドル（失敗時は `kInvalidSoundHandle`）
    static SoundHandle LoadDynamic(const std::string &filePath);
#endif

    const std::string& GetAssetsRootPath() const noexcept { return assetsRootPath_; }

    /// @brief 再生中の音声の現在位置を秒単位で取得する
    /// @param play 再生ハンドル
    /// @param outSeconds 取得した秒数の出力先
    /// @return 成功した場合 true
    static bool GetPlayPositionSeconds(PlayHandle play, double& outSeconds);

    static void RegisterSoundBeat(Passkey<SoundBeat>, SoundBeat* soundBeat);
    static void UnregisterSoundBeat(Passkey<SoundBeat>, SoundBeat* soundBeat);

    static void RegisterAudioPlayer(Passkey<AudioPlayer>, AudioPlayer* player);
    static void UnregisterAudioPlayer(Passkey<AudioPlayer>, AudioPlayer* player);

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
