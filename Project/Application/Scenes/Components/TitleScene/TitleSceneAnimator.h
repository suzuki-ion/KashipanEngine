#pragma once
#include <KashipanEngine.h>
#include "Scenes/Components/TitleScene/CarMove.h"
#include "Scenes/Components/TitleScene/PlayerEnter.h"
#include "Scenes/Components/TitleScene/StartTextUpdate.h"
#include "Scenes/Components/TitleScene/CameraStartMovement.h"

namespace KashipanEngine {

class TitleSceneAnimator final : public ISceneComponent {
public:
    using RegisterFunc = std::function<bool(std::unique_ptr<ISceneComponent>)>;

    TitleSceneAnimator(RegisterFunc regFunc, InputCommand *ic)
        : ISceneComponent("TitleSceneAnimator"), registerFunc_(regFunc), inputCommand_(ic) {
        if (registerFunc_) {
            registerFunc_(std::make_unique<CameraStartMovement>());
            registerFunc_(std::make_unique<CarMove>());
            registerFunc_(std::make_unique<PlayerEnter>());
            registerFunc_(std::make_unique<StartTextUpdate>(inputCommand_));
        }
    }

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        cameraMovement_ = ctx->GetComponent<CameraStartMovement>();
        carMove_ = ctx->GetComponent<CarMove>();
        playerEnter_ = ctx->GetComponent<PlayerEnter>();
        startTextUpdate_ = ctx->GetComponent<StartTextUpdate>();

        auto *sceneDefaultVariables = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer3D = sceneDefaultVariables ? sceneDefaultVariables->GetScreenBuffer3D() : nullptr;

        letterbox_ = ctx->GetComponent<Letterbox>();
        if (letterbox_) {
            letterbox_->SetThickness(0.0f);
        }

        // タイトルロゴ
        {
            auto modelHandle = ModelManager::GetModelDataFromFileName("title.obj");
            auto obj = std::make_unique<Model>(modelHandle);
            obj->SetName("TitleLogo");
            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3(0.0f, 48.0f, 10.0f));
                tr->SetRotate(Vector3(0.0f, 0.0f, 0.0f));
                tr->SetScale(Vector3(titleLogoScaleMax_, titleLogoScaleMax_, titleLogoScaleMax_));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            titleLogoModel_ = obj.get();
            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            ctx->AddObject3D(std::move(obj));
        }

        // BGM 再生
        bgmHandle_ = AudioManager::GetSoundHandleFromFileName("titleBGM.mp3");
        if (bgmHandle_ != AudioManager::kInvalidSoundHandle) {
            bgmPlayHandle_ = AudioManager::Play(bgmHandle_, bgmBaseVolume_, 0.0f, true);
        }

        bpmHandle_ = AudioManager::GetSoundHandleFromFileName("BPM.mp3");
        soundBeat_.SetBeat(AudioManager::kInvalidPlayHandle, 90.0f, 0.0);
        soundBeat_.StartManualBeat();
    }

    void Finalize() override {
        // BGM 停止
        if (bgmPlayHandle_ != AudioManager::kInvalidPlayHandle) {
            AudioManager::Stop(bgmPlayHandle_);
            bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;
        }
        soundBeat_.SetPlayHandle(AudioManager::kInvalidPlayHandle);
    }

    void Update() override {
        if (!inputCommand_) return;

#if defined(DEBUG_BUILD)
        if (inputCommand_->Evaluate("DebugReset").Triggered()) {
            if (carMove_) carMove_->Reset();
            if (playerEnter_) playerEnter_->Reset();
            if (startTextUpdate_) startTextUpdate_->Reset();
            if (cameraMovement_) cameraMovement_->Reset();
            if (letterbox_) letterbox_->SetThickness(0.0f);
            letterboxAnimating_ = false;
            letterboxElapsed_ = 0.0f;
            isAnimating_ = false;
            return;
        }
#endif

        if (startTextUpdate_) {
            if (startTextUpdate_->IsFinishedTriggered()) {
                if (carMove_) carMove_->StartMoveIn();
                StartBgmFade();
                StartLetterboxAnimation();
                isAnimating_ = true;
            }
        }

        if (carMove_) {
            if (carMove_->IsFinishedTriggered()) {
                if (carMove_->IsMoveIn()) {
                    if (playerEnter_) playerEnter_->StartAppearance();
                    carMove_->StartMoveOut();
                    CompleteBgmFade();
                    isAnimating_ = true;
                }
            }
        }

        if (playerEnter_) {
            if (playerEnter_->IsFinishedTriggered()) {
                if (playerEnter_->IsPlayerAppearance()) {
                    if (cameraMovement_) cameraMovement_->StartAnimation();
                    StartBpmSound();
                    playerEnter_->StartEnter();
                    isAnimating_ = true;
                }
            }
        }

        if (cameraMovement_) {
            if (cameraMovement_->IsAnimating()) {
                isAnimating_ = true;
            }
        }

        UpdateBgmFade();
        UpdateBpmFade();
        UpdateTitleLogoScale();
        UpdateLetterboxAnimation();

        if (soundBeat_.IsOnBeatTriggered()) {
            // BPM音再生
            if (bpmHandle_ != AudioManager::kInvalidSoundHandle) {
                AudioManager::Play(bpmHandle_, bpmVolume_, 0.0f, false);
            }
        }
    }

    bool IsAnimating() const {
        return isAnimating_;
    }

    bool IsAnimationFinished() const {
        if (cameraMovement_) {
            return cameraMovement_->IsFinished();
        }
        return false;
    }
    bool IsAnimationFinishedTriggered() const {
        if (cameraMovement_) {
            return cameraMovement_->IsFinishedTriggered();
        }
        return false;
    }

private:
    void StartBgmFade() {
        if (bgmPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        if (isBgmFadeActive_) return;
        isBgmFadeActive_ = true;
        bgmFadeElapsed_ = 0.0f;
    }

    void CompleteBgmFade() {
        if (bgmPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        if (!isBgmFadeActive_) return;
        AudioManager::SetVolume(bgmPlayHandle_, bgmFadeTargetVolume_);
        isBgmFadeActive_ = false;
        bgmFadeElapsed_ = bgmFadeDuration_;
    }

    void UpdateBgmFade() {
        if (!isBgmFadeActive_) return;
        if (bgmPlayHandle_ == AudioManager::kInvalidPlayHandle) return;
        bgmFadeElapsed_ += GetDeltaTime();
        const float t = Normalize01(bgmFadeElapsed_, 0.0f, bgmFadeDuration_);
        const float volume = Lerp(bgmBaseVolume_, bgmFadeTargetVolume_, t);
        AudioManager::SetVolume(bgmPlayHandle_, volume);
        if (t >= 1.0f) {
            isBgmFadeActive_ = false;
        }
    }

    void StartBpmSound() {
        bpmFadeElapsed_ = 0.0f;
        isBpmFadeActive_ = true;
    }

    void UpdateBpmFade() {
        if (!isBpmFadeActive_) return;
        bpmFadeElapsed_ += GetDeltaTime();
        const float t = Normalize01(bpmFadeElapsed_, 0.0f, bpmFadeDuration_);
        const float volume = Lerp(0.0f, bpmTargetVolume_, t);
        bpmVolume_ = volume;
        if (t >= 1.0f) {
            isBpmFadeActive_ = false;
        }
    }

    void UpdateTitleLogoScale() {
        if (!titleLogoModel_) return;
        if (auto *tr = titleLogoModel_->GetComponent3D<Transform3D>()) {
            const float beatProgress = soundBeat_.GetBeatProgress();
            const float scale = EaseOutCubic(titleLogoScaleMin_, titleLogoScaleMax_, beatProgress);
            tr->SetScale(Vector3(scale, scale, scale));
        }
    }

    void StartLetterboxAnimation() {
        if (!letterbox_) return;
        letterboxAnimating_ = true;
        letterboxElapsed_ = 0.0f;
        letterbox_->SetThickness(0.0f);
    }

    void UpdateLetterboxAnimation() {
        if (!letterboxAnimating_ || !letterbox_) return;
        letterboxElapsed_ += GetDeltaTime();
        const float t = Normalize01(letterboxElapsed_, 0.0f, letterboxDuration_);
        const float thickness = EaseOutCubic(0.0f, letterboxTargetThickness_, t);
        letterbox_->SetThickness(thickness);
        if (t >= 1.0f) {
            letterboxAnimating_ = false;
        }
    }

    RegisterFunc registerFunc_;
    InputCommand *inputCommand_ = nullptr;

    CarMove *carMove_ = nullptr;
    PlayerEnter *playerEnter_ = nullptr;
    StartTextUpdate *startTextUpdate_ = nullptr;
    CameraStartMovement *cameraMovement_ = nullptr;
    Letterbox *letterbox_ = nullptr;

    Model *titleLogoModel_ = nullptr;

    AudioManager::SoundHandle bgmHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::PlayHandle bgmPlayHandle_ = AudioManager::kInvalidPlayHandle;

    AudioManager::SoundHandle bpmHandle_ = AudioManager::kInvalidSoundHandle;
    AudioManager::SoundBeat soundBeat_{};

    float bgmFadeElapsed_ = 0.0f;
    float bpmFadeElapsed_ = 0.0f;
    float bpmVolume_ = 0.0f;
    bool isBgmFadeActive_ = false;
    bool isBpmFadeActive_ = false;

    float letterboxElapsed_ = 0.0f;
    bool letterboxAnimating_ = false;

    const float bgmBaseVolume_ = 0.5f;
    const float bgmFadeTargetVolume_ = bgmBaseVolume_ * 0.3f;
    const float bgmFadeDuration_ = 4.0f;

    const float bpmTargetVolume_ = 1.0f;
    const float bpmFadeDuration_ = 8.0f;

    const float letterboxDuration_ = 1.0f;
    const float letterboxTargetThickness_ = 64.0f;

    const float titleLogoScaleMin_ = 0.8f;
    const float titleLogoScaleMax_ = 1.0f;

    bool isAnimating_ = false;
};

} // namespace KashipanEngine
