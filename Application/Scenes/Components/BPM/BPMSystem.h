#pragma once
#include <KashipanEngine.h>
#include <cmath>

namespace KashipanEngine {

    class BPMSystem final : public ISceneComponent {
    public:
        explicit BPMSystem(float bpm = 120.0f)
            : ISceneComponent("BPMSystem", 1) {
            SetBPM(bpm);
        }

        ~BPMSystem() override = default;

        void Initialize() override {
            ISceneComponent::Initialize();
            elapsedTime_ = 0.0f;
            currentBeat_ = 0;
        }

        void Update() override {
            if (beatDuration_ <= 0.0f) return; // BPM未設定/無効なら無視

            const float deltaTime = GetDeltaTime();
            elapsedTime_ += deltaTime;

            // フレーム内で複数拍またいだ場合も確実に発火させ、elapsedTime_ を小さく保つ
            while (elapsedTime_ >= beatDuration_) {
                elapsedTime_ -= beatDuration_;
                ++currentBeat_;
                OnBeat();
            }
        }

        /// @brief 現在の拍内での進行度を取得 (0.0 ~ 1.0)
        float GetBeatProgress() const {
            if (beatDuration_ <= 0.0f) return 0.0f;
            return std::clamp(elapsedTime_ / beatDuration_, 0.0f, 1.0f);
        }

        /// @brief 現在の拍番号を取得
        int GetCurrentBeat() const { return currentBeat_; }

        /// @brief BPMを設定
        void SetBPM(float bpm) {
            if (bpm <= 0.0f) {
                // 無効なBPMは無視（もしくはデフォルトに戻す）
                return;
            }
            // elapsedTime_ は拍内経過時間として保持しているため、値の比率を保つ
            if (beatDuration_ > 0.0f) {
                const float progress = elapsedTime_ / beatDuration_;
                bpm_ = bpm;
                beatDuration_ = 60.0f / bpm_;
                elapsedTime_ = std::clamp(progress * beatDuration_, 0.0f, beatDuration_);
            } else {
                bpm_ = bpm;
                beatDuration_ = 60.0f / bpm_;
            }
        }

        /// @brief BPMを取得
        float GetBPM() const { return bpm_; }

		/// @brief 1拍の時間を取得（秒）
		float GetBeatDuration() const { return beatDuration_; }
    private:

        /// @brief 拍が発生したときの処理
        void OnBeat() {
            auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Sounds/rhythm.wav"); // BPM.mp3
            if (handle == AudioManager::kInvalidSoundHandle) {
                // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
                return;
            }
            AudioManager::Play(handle, 0.05f);
        }

        float bpm_ = 120.0f;
        float beatDuration_ = 0.0f;
        float elapsedTime_ = 0.0f; // 常に 0 <= elapsedTime_ < beatDuration_ を維持
        int currentBeat_ = 0;
    };

} // namespace KashipanEngine