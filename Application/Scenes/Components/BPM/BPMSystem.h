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
            currentBeat_ = 0;
        }

        void Update() override {
            if (beatDuration_ <= 0.0f) return; // BPM未設定/無効なら無視

            bpmTimer_.Update();

            // タイマーが完了した瞬間のみ拍を進める
            if (bpmTimer_.IsFinished()) {
				leftRightToggle_ = !leftRightToggle_;
                ++currentBeat_;
                OnBeat();
            }
        }

        void MeasurementStart() {
            bpmTimer_.Start(beatDuration_, true);
        }

        /// @brief 現在の拍内での進行度を取得 (0.0 ~ 1.0)
        float GetBeatProgress() const {
			return bpmTimer_.GetProgress();
        }

        /// @brief 現在の拍番号を取得
        int GetCurrentBeat() const { return currentBeat_; }

        /// @brief BPMを設定
        void SetBPM(float bpm) {
            if (bpm <= 0.0f) {
                // 無効なBPMは無視（もしくはデフォルトに戻す）
                return;
            }
            
            bpm_ = bpm;
            beatDuration_ = 60.0f / bpm_;
			bpmTimer_.SetDuration(beatDuration_);
        }

        /// @brief BPMを取得
        float GetBPM() const { return bpm_; }

		/// @brief 1拍の時間を取得（秒）
		float GetBeatDuration() const { return beatDuration_; }

		/// @brief 現在の拍が発生したかどうかを取得
        bool GetOnBeat() const {
            return bpmTimer_.IsFinished();
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

		GameTimer bpmTimer_;
        float bpm_ = 120.0f;
        float beatDuration_ = 0.0f;
        int currentBeat_ = 0;

		bool leftRightToggle_ = false;
    };

} // namespace KashipanEngine