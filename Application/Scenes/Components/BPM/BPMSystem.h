#pragma once
#include <KashipanEngine.h>
#include <cmath>

namespace KashipanEngine {

    class BPMSystem final : public ISceneComponent {
    public:
        explicit BPMSystem(AudioManager::PlayHandle bgmPlayHandle = AudioManager::kInvalidPlayHandle, float bpm = 120.0f, double beatStartOffset = 0.0)
            : ISceneComponent("BPMSystem", 1) {
            bgmPlayHandle_ = bgmPlayHandle;
            bpm_ = bpm;
            beatStartOffset_ = beatStartOffset;
            soundBeat_.SetBeat(bgmPlayHandle_, bpm_, beatStartOffset_);
        }

        ~BPMSystem() override = default;

        void Initialize() override {
            soundBeat_.SetOnBeat([this](auto, auto, auto) {
                OnBeat();
            });
        }

        void Update() override {
            if (soundBeat_.IsOnBeatTriggered()) {
                leftRightToggle_ = !leftRightToggle_;
            }
        }

		/// @brief 測定開始
        void MeasurementStart(AudioManager::PlayHandle bgmPlayHandle = AudioManager::kInvalidPlayHandle, float bpm = 120.0f, double beatStartOffset = 0.0) {
            bgmPlayHandle_ = bgmPlayHandle == AudioManager::kInvalidPlayHandle ? bgmPlayHandle_ : bgmPlayHandle;
            bpm_ = bpm_ <= 0.0f ? bpm_ : bpm;
            beatStartOffset_ = beatStartOffset < 0.0 ? beatStartOffset_ : beatStartOffset;
            soundBeat_.SetBeat(bgmPlayHandle_, bpm_, beatStartOffset_);
        }

		/// @brief システムリセット
        void ResetSystem() {
            soundBeat_.Reset();
		}

        /// @brief 現在の拍内での進行度を取得 (0.0 ~ 1.0)
        float GetBeatProgress() const {
            return soundBeat_.GetBeatProgress();
        }

        /// @brief 現在の拍番号を取得
        int GetCurrentBeat() const { return static_cast<int>(soundBeat_.GetCurrentBeat()); }

        /// @brief ビート情報の設定
        /// @param bgmPlayHandle BGMの再生ハンドル
        /// @param bpm BPM値
        /// @param beatStartOffset ビート開始オフセット（秒）
        void SetBeat(AudioManager::PlayHandle bgmPlayHandle, float bpm, double beatStartOffset = 0.0) {
            bgmPlayHandle_ = bgmPlayHandle;
            bpm_ = bpm;
            beatStartOffset_ = beatStartOffset;
            soundBeat_.SetBeat(bgmPlayHandle_, bpm_, beatStartOffset_);
        }

        /// @brief BPMを設定
        void SetBPM(float bpm) {
            if (bpm <= 0.0f) {
                // 無効なBPMは無視（もしくはデフォルトに戻す）
                return;
            }
            bpm_ = bpm;
            soundBeat_.SetBPM(bpm_);
        }

        /// @brief BPMを取得
        float GetBPM() const { return bpm_; }

		/// @brief 1拍の時間を取得（秒）
        float GetBeatDuration() const { return 60.0f / bpm_; }

		/// @brief 現在の拍が発生したかどうかを取得
        bool GetOnBeat() const {
            return soundBeat_.IsOnBeatTriggered();
		}

		/// @brief 左右トグル状態を取得 true: 左, false: 右
        bool GetLeftRightToggle() const {
            return leftRightToggle_;
		}
    private:

        /// @brief 拍が発生したときの処理
        void OnBeat() {
            auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Sounds/rhythm.wav"); // BPM.mp3
            if (handle == AudioManager::kInvalidSoundHandle) {
                // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
                return;
            }
            AudioManager::Play(handle, 0.2f);
        }

        AudioManager::SoundBeat soundBeat_;
        AudioManager::PlayHandle bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;
        float bpm_ = 120.0f;
        double beatStartOffset_ = 0.0;

		bool leftRightToggle_ = false;
    };

} // namespace KashipanEngine