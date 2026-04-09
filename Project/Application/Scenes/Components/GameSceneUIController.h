#pragma once
#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageGroundGenerator.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>
#include <cmath>

namespace KashipanEngine {

class GameSceneUIController final : public ISceneComponent {
public:
    GameSceneUIController() : ISceneComponent("GameSceneUIController", 1) {}
    ~GameSceneUIController() override = default;

    void StartClearPresentation(int touchedGroundCount) {
        clearTouchedGroundCount_ = touchedGroundCount;
        clearPresentationActive_ = true;
        clearPresentationElapsed_ = 0.0f;
        clearReturnRequested_ = false;

        if (clearResultText_) {
            clearResultText_->SetText(" ");
        }
    }

    bool IsClearPresentationFinished() const {
        return clearPresentationActive_ && clearPresentationElapsed_ >= clearPresentationDuration_;
    }

    bool ConsumeRequestedReturnToTitle() {
        const bool requested = clearReturnRequested_;
        clearReturnRequested_ = false;
        return requested;
    }

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaultVars = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars) return;

        auto *screenBuffer2D = defaultVars->GetScreenBuffer2D();
        if (!screenBuffer2D) return;

        mainCamera_ = defaultVars->GetMainCamera3D();
        screenWidth_ = static_cast<float>(screenBuffer2D->GetWidth());
        screenHeight_ = static_cast<float>(screenBuffer2D->GetHeight());

        auto speedBar = std::make_unique<SpriteProressBar>();
        speedBar->SetName("ForwardSpeedBar");
        speedBar->SetBarSize(Vector2{512.0f, 32.0f});
        speedBar->SetFrameThickness(8.0f);
        speedBar->SetFrameColor(Vector4{0.5f, 0.5f, 0.5f, 1.0f});
        speedBar->SetBackgroundColor(Vector4{0.1f, 0.1f, 0.1f, 1.0f});
        speedBar->SetBarColor(Vector4{0.0f, 0.5f, 0.0f, 1.0f});
        speedBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = speedBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{320.0f, 32.0f, 0.0f});
        }
        forwardSpeedBar_ = speedBar.get();
        (void)ctx->AddObject2D(std::move(speedBar));

        auto gravityBar = std::make_unique<SpriteProressBar>();
        gravityBar->SetName("GravityGaugeBar");
        gravityBar->SetBarSize(Vector2{512.0f, 20.0f});
        gravityBar->SetFrameThickness(6.0f);
        gravityBar->SetFrameColor(Vector4{0.35f, 0.35f, 0.55f, 1.0f});
        gravityBar->SetBackgroundColor(Vector4{0.08f, 0.08f, 0.1f, 1.0f});
        gravityBar->SetBarColor(Vector4{0.2f, 0.6f, 1.0f, 1.0f});
        gravityBar->SetSegmentLineCount(1);
        gravityBar->SetSegmentLineColor(Vector4{0.5f, 0.5f, 0.5f, 1.0f});
        gravityBar->SetSegmentLineThickness(4.0f);
        gravityBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = gravityBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{320.0f, 70.0f, 0.0f});
        }
        gravityGaugeBar_ = gravityBar.get();
        (void)ctx->AddObject2D(std::move(gravityBar));

        auto speedText = std::make_unique<Text>(128);
        speedText->SetName("ForwardSpeedText");
        speedText->SetFont("Assets/Application/Image/test.fnt");
        speedText->SetTextFormat("Speed: {0:.2f}", 0.0f);
        speedText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        forwardSpeedText_ = speedText.get();
        (void)ctx->AddObject2D(std::move(speedText));

        auto touchedGroundText = std::make_unique<Text>(128);
        touchedGroundText->SetName("TouchedGroundCountText");
        touchedGroundText->SetFont("Assets/Application/Image/test.fnt");
        touchedGroundText->SetTextFormat("Touched Ground: {0}", 0);
        touchedGroundText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = touchedGroundText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, std::max(32.0f, screenHeight_ - 48.0f), 0.0f});
        }
        touchedGroundCountText_ = touchedGroundText.get();
        (void)ctx->AddObject2D(std::move(touchedGroundText));

        auto landingTouchedGroundText = std::make_unique<Text>(64);
        landingTouchedGroundText->SetName("LandingTouchedGroundCountText");
        landingTouchedGroundText->SetFont("Assets/Application/Image/test.fnt");
        landingTouchedGroundText->SetText(" ");
        landingTouchedGroundText->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        landingTouchedGroundText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        landingTouchedGroundCountText_ = landingTouchedGroundText.get();
        (void)ctx->AddObject2D(std::move(landingTouchedGroundText));

        auto fallDistanceText = std::make_unique<Text>(128);
        fallDistanceText->SetName("FallDistanceText");
        fallDistanceText->SetFont("Assets/Application/Image/test.fnt");
        fallDistanceText->SetTextFormat("Fall Distance: {0:.2f}", 0.0f);
        fallDistanceText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = fallDistanceText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, 150.0f, 0.0f});
        }
        fallDistanceText_ = fallDistanceText.get();
        (void)ctx->AddObject2D(std::move(fallDistanceText));

        auto clearFade = std::make_unique<Sprite>();
        clearFade->SetName("ClearFadeSprite");
        clearFade->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = clearFade->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{screenWidth_ * 0.5f, screenHeight_ * 0.5f, 0.0f});
            tr->SetScale(Vector3{screenWidth_, screenHeight_, 1.0f});
        }
        if (auto *mat = clearFade->GetComponent2D<Material2D>()) {
            mat->SetTexture(TextureManager::kInvalidHandle);
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 0.0f});
        }
        clearFadeSprite_ = clearFade.get();
        (void)ctx->AddObject2D(std::move(clearFade));

        auto clearText = std::make_unique<Text>(128);
        clearText->SetName("ClearTouchedGroundText");
        clearText->SetFont("Assets/Application/Image/test.fnt");
        clearText->SetText(" ");
        clearText->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        clearText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = clearText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{screenWidth_ * 0.5f, screenHeight_ * 0.5f, 0.0f});
        }
        for (size_t i = 0; i < 128; ++i) {
            if (auto *sprite = (*clearText)[i]) {
                if (auto *mat = sprite->GetComponent2D<Material2D>()) {
                    mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.0f});
                }
            }
        }
        clearResultText_ = clearText.get();
        (void)ctx->AddObject2D(std::move(clearText));
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("Player");
        }
        if (player_ && !playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (!stageGroundGenerator_) {
            stageGroundGenerator_ = ctx->GetComponent<StageGroundGenerator>();
        }

        const int touchedCount = stageGroundGenerator_ ? stageGroundGenerator_->GetTouchedGroundCount() : 0;
        const int touchedDelta = std::max(0, touchedCount - previousTouchedGroundCount_);
        previousTouchedGroundCount_ = touchedCount;

        float landingImpact = 0.0f;
        if (playerMovementController_ && playerMovementController_->ConsumeLandingImpact(landingImpact)) {
            landingPopupRequestTimer_ = landingPopupRequestDuration_;
            landingPopupPending_ = true;
            landingPopupPendingStartTouchedCount_ = touchedCount;
        }

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        landingPopupRequestTimer_ = std::max(0.0f, landingPopupRequestTimer_ - dt);
        landingPopupRemainTime_ = std::max(0.0f, landingPopupRemainTime_ - dt);

        if (landingPopupPending_) {
            const int pendingDelta = std::max(0, touchedCount - landingPopupPendingStartTouchedCount_);
            if (pendingDelta > 0) {
                landingTouchedGroundCount_ = pendingDelta;
                landingPopupRemainTime_ = landingPopupShowDuration_;
                landingPopupRequestTimer_ = 0.0f;
                landingPopupPending_ = false;
            }
        } else if (touchedDelta > 0 && landingPopupRequestTimer_ > 0.0f) {
            landingTouchedGroundCount_ = touchedDelta;
            landingPopupRemainTime_ = landingPopupShowDuration_;
            landingPopupRequestTimer_ = 0.0f;
        }

        if (forwardSpeedBar_ && playerMovementController_) {
            const float speed = playerMovementController_->GetForwardSpeed();
            const float minSpeed = playerMovementController_->GetMinForwardSpeed();
            const float maxSpeed = playerMovementController_->GetMaxForwardSpeed();
            const float range = std::max(0.0001f, maxSpeed - minSpeed);
            const float progress = std::clamp((speed - minSpeed) / range, 0.0f, 1.0f);
            forwardSpeedBar_->SetProgress(progress);
            if (forwardSpeedText_) {
                forwardSpeedText_->SetTextFormat("Speed: {0:.2f}", speed);
            }
        }

        if (gravityGaugeBar_ && playerMovementController_) {
            gravityGaugeBar_->SetProgress(playerMovementController_->GetGravityGaugeNormalized());
        }

        if (touchedGroundCountText_) {
            touchedGroundCountText_->SetTextFormat("Touched Ground: {0}", touchedCount);
        }

        if (landingTouchedGroundCountText_) {
            if (landingPopupRemainTime_ > 0.0f) {
                landingTouchedGroundCountText_->SetTextFormat("+{0}", landingTouchedGroundCount_);
                const float alpha = std::clamp(landingPopupRemainTime_ / std::max(0.0001f, landingPopupShowDuration_), 0.0f, 1.0f);
                SetTextAlpha(landingTouchedGroundCountText_, alpha);
                if (player_) {
                    if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
                        if (auto *tr = landingTouchedGroundCountText_->GetComponent2D<Transform2D>()) {
                            const Vector3 playerPos = playerTr->GetTranslate();
                            Vector2 projectPos;
                            if (ProjectWorldTo2DWorld(playerPos, projectPos)) {
                                tr->SetTranslate(Vector3{ projectPos.x, projectPos.y + touchedGroundPopupYOffset_, 0.0f });
                            }
                        }
                    }
                }
            } else {
                landingTouchedGroundCountText_->SetText(" ");
                SetTextAlpha(landingTouchedGroundCountText_, 0.0f);
            }
        }

        if (fallDistanceText_ && playerMovementController_) {
            fallDistanceText_->SetTextFormat("Fall Distance: {0:.2f}", playerMovementController_->GetAccumulatedFallDistance());
        }

        if (clearPresentationActive_) {
            clearPresentationElapsed_ += dt;
            const float fadeT = std::clamp(clearPresentationElapsed_ / std::max(0.0001f, clearPresentationDuration_), 0.0f, 1.0f);

            if (clearFadeSprite_) {
                if (auto *mat = clearFadeSprite_->GetComponent2D<Material2D>()) {
                    mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, fadeT});
                }
            }

            if (clearResultText_) {
                if (clearPresentationElapsed_ >= clearPresentationDuration_) {
                    clearResultText_->SetTextFormat("Touched Ground: {0}", clearTouchedGroundCount_);
                    SetTextAlpha(clearResultText_, 1.0f);
                    if (auto *ic = ctx->GetInputCommand()) {
                        if (ic->Evaluate("Submit").Triggered()) {
                            clearReturnRequested_ = true;
                        }
                    }
                } else {
                    clearResultText_->SetText(" ");
                    SetTextAlpha(clearResultText_, 0.0f);
                }
            }
        } else {
            if (clearFadeSprite_) {
                if (auto *mat = clearFadeSprite_->GetComponent2D<Material2D>()) {
                    mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 0.0f});
                }
            }
            if (clearResultText_) {
                clearResultText_->SetText(" ");
                SetTextAlpha(clearResultText_, 0.0f);
            }
        }
    }

private:
    bool ProjectWorldTo2DWorld(const Vector3 &world, Vector2 &out) const {
        if (!mainCamera_ || screenWidth_ <= 0.0f || screenHeight_ <= 0.0f) return false;

        const Matrix4x4 &viewProj = mainCamera_->GetViewProjectionMatrix();
        const float clipX = world.x * viewProj.m[0][0] + world.y * viewProj.m[1][0] + world.z * viewProj.m[2][0] + viewProj.m[3][0];
        const float clipY = world.x * viewProj.m[0][1] + world.y * viewProj.m[1][1] + world.z * viewProj.m[2][1] + viewProj.m[3][1];
        const float clipW = world.x * viewProj.m[0][3] + world.y * viewProj.m[1][3] + world.z * viewProj.m[2][3] + viewProj.m[3][3];
        if (std::abs(clipW) <= 0.000001f) return false;

        const float ndcX = clipX / clipW;
        const float ndcY = clipY / clipW;
        out.x = (ndcX + 1.0f) * 0.5f * screenWidth_;
        out.y = (ndcY + 1.0f) * 0.5f * screenHeight_;
        return true;
    }

    void SetTextAlpha(Text *text, float alpha) {
        if (!text) return;
        for (size_t i = 0; i < 64; ++i) {
            Sprite *sprite = (*text)[i];
            if (!sprite) continue;
            auto *mat = sprite->GetComponent2D<Material2D>();
            if (!mat) continue;
            Vector4 color = mat->GetColor();
            color.w = alpha;
            mat->SetColor(color);
        }
    }

    Object3DBase *player_ = nullptr;
    PlayerMovementController *playerMovementController_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;
    Camera3D *mainCamera_ = nullptr;

    SpriteProressBar *forwardSpeedBar_ = nullptr;
    SpriteProressBar *gravityGaugeBar_ = nullptr;
    Text *forwardSpeedText_ = nullptr;
    Text *touchedGroundCountText_ = nullptr;
    Text *fallDistanceText_ = nullptr;
    Text *landingTouchedGroundCountText_ = nullptr;
    Sprite *clearFadeSprite_ = nullptr;
    Text *clearResultText_ = nullptr;

    int previousTouchedGroundCount_ = 0;
    int landingTouchedGroundCount_ = 0;
    bool landingPopupPending_ = false;
    int landingPopupPendingStartTouchedCount_ = 0;
    float landingPopupRemainTime_ = 0.0f;
    float landingPopupRequestTimer_ = 0.0f;

    float landingPopupShowDuration_ = 2.0f;
    float landingPopupRequestDuration_ = 0.25f;
    float touchedGroundPopupYOffset_ = 128.0f;

    bool clearPresentationActive_ = false;
    bool clearReturnRequested_ = false;
    int clearTouchedGroundCount_ = 0;
    float clearPresentationElapsed_ = 0.0f;
    float clearPresentationDuration_ = 1.0f;

    float screenWidth_ = 0.0f;
    float screenHeight_ = 0.0f;
};

} // namespace KashipanEngine
