#pragma once

#include <KashipanEngine.h>

namespace KashipanEngine {

class TitleSceneAudioPlayer final : public ISceneComponent {
public:
    TitleSceneAudioPlayer()
        : ISceneComponent("TitleSceneAudioPlayer", 1) {}

    ~TitleSceneAudioPlayer() override = default;

    void Initialize() override {
        bgmSoundHandle_ = AudioManager::GetSoundHandleFromFileName("bgmTitleScene.mp3");
        selectSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISelect.mp3");
        submitSeSoundHandle_ = AudioManager::GetSoundHandleFromFileName("seUISubmit.mp3");

        if (bgmSoundHandle_ != AudioManager::kInvalidSoundHandle) {
            bgmPlayHandle_ = AudioManager::Play(bgmSoundHandle_, 0.5f, 0.0f, true);
        }
    }

    void Finalize() override {
        if (bgmPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            (void)AudioManager::Stop(bgmPlayHandle_);
            bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;
        }
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *inputCommand = ctx->GetInputCommand();
        if (!inputCommand) return;

        if (inputCommand->Evaluate("SelectUp").Triggered() || inputCommand->Evaluate("SelectDown").Triggered()) {
            PlaySE(selectSeSoundHandle_);
        }

        if (inputCommand->Evaluate("Submit").Triggered()) {
            PlaySE(submitSeSoundHandle_, currentPitch_);
            currentPitch_ += 1.0f;
			currentPitchResetTime_ = pitchResetTime_;
        }

        if(currentPitchResetTime_ > 0.0f) {
            currentPitchResetTime_ -= GetDeltaTime();
        } else {
			currentPitch_ = 0.0f;
        }
    }

private:
    static void PlaySE(AudioManager::SoundHandle handle, float pitch = 0.0f, float volume = 1.0f) {
        if (handle == AudioManager::kInvalidSoundHandle) return;
        (void)AudioManager::Play(handle, volume, pitch, false);
    }

private:
    AudioManager::SoundHandle bgmSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle selectSeSoundHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundHandle submitSeSoundHandle_ = AudioManager::kInvalidSoundHandle;

    AudioManager::PlayHandle bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;

	float pitchResetTime_ = 0.3f;
	float currentPitchResetTime_ = 0.0f;
	float currentPitch_ = 0.0f;
};

} // namespace KashipanEngine
