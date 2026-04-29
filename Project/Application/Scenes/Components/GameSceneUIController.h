#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageGoalPlaneController.h"
#include "Objects/Components/PlayerMovementController.h"
#include "Objects/Components/PlayerInputHandler.h"

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

    void SetVisible(bool visible) {
        if (isVisible_ == visible) return;
        isVisible_ = visible;
        ApplyVisibility();
    }

    bool IsVisible() const { return isVisible_; }

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
            gravityGaugeBarBasePosition_ = tr->GetTranslate();
        }
        gravityGaugeBar_ = gravityBar.get();
        (void)ctx->AddObject2D(std::move(gravityBar));

        auto goalDistanceBar = std::make_unique<SpriteProressBar>();
        goalDistanceBar->SetName("GoalDistanceBar");
        goalDistanceBar->SetBarSize(Vector2{28.0f, 320.0f});
        goalDistanceBar->SetFillDirection(SpriteProressBar::FillDirection::BottomToTop);
        goalDistanceBar->SetFrameThickness(6.0f);
        goalDistanceBar->SetFrameColor(Vector4{0.4f, 0.4f, 0.4f, 1.0f});
        goalDistanceBar->SetBackgroundColor(Vector4{0.08f, 0.08f, 0.08f, 1.0f});
        goalDistanceBar->SetBarColor(Vector4{0.95f, 0.85f, 0.2f, 1.0f});
        goalDistanceBar->SetSegmentLineCount(3);
        goalDistanceBar->SetSegmentLineColor(Vector4{0.6f, 0.6f, 0.6f, 1.0f});
        goalDistanceBar->SetSegmentLineThickness(3.0f);
        goalDistanceBar->SetProgress(0.0f);
        goalDistanceBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = goalDistanceBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{std::max(32.0f, screenWidth_ - 48.0f), screenHeight_ * 0.5f, 0.0f});
        }
        goalDistanceBar_ = goalDistanceBar.get();
        (void)ctx->AddObject2D(std::move(goalDistanceBar));

        auto speedText = std::make_unique<Text>(128);
        speedText->SetName("ForwardSpeedText");
        speedText->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        speedText->SetTextFormat("Speed: {0:.2f}", 0.0f);
        speedText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        forwardSpeedText_ = speedText.get();
        (void)ctx->AddObject2D(std::move(speedText));

        auto touchedGroundText = std::make_unique<Text>(128);
        touchedGroundText->SetName("TouchedGroundCountText");
        touchedGroundText->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        touchedGroundText->SetTextFormat("Touched Ground: {0}", 0);
        touchedGroundText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = touchedGroundText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, std::max(32.0f, screenHeight_ - 48.0f), 0.0f});
        }
        touchedGroundCountText_ = touchedGroundText.get();
        (void)ctx->AddObject2D(std::move(touchedGroundText));

        auto landingTouchedGroundText = std::make_unique<Text>(64);
        landingTouchedGroundText->SetName("LandingTouchedGroundCountText");
        landingTouchedGroundText->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        landingTouchedGroundText->SetText(" ");
        landingTouchedGroundText->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        landingTouchedGroundText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        landingTouchedGroundCountText_ = landingTouchedGroundText.get();
        (void)ctx->AddObject2D(std::move(landingTouchedGroundText));

        auto fallDistanceText = std::make_unique<Text>(128);
        fallDistanceText->SetName("FallDistanceText");
        fallDistanceText->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        fallDistanceText->SetTextFormat("Fall Distance: {0:.2f}", 0.0f);
        fallDistanceText->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = fallDistanceText->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{32.0f, 150.0f, 0.0f});
        }
        fallDistanceText_ = fallDistanceText.get();
        (void)ctx->AddObject2D(std::move(fallDistanceText));

        auto jumpRemainGaugeBar = std::make_unique<SpriteProressBar>();
        jumpRemainGaugeBar->SetName("JumpRemainGaugeBar");
        jumpRemainGaugeBar->SetBarSize(Vector2{160.0f, 16.0f});
        jumpRemainGaugeBar->SetFrameThickness(4.0f);
        jumpRemainGaugeBar->SetFrameColor(jumpRemainFrameColorBase_);
        jumpRemainGaugeBar->SetBackgroundColor(jumpRemainBackgroundColorBase_);
        jumpRemainGaugeBar->SetBarColor(jumpRemainBarColorBase_);
        jumpRemainGaugeBar->SetSegmentLineCount(1);
        jumpRemainGaugeBar->SetSegmentLineColor(jumpRemainSegmentColorBase_);
        jumpRemainGaugeBar->SetSegmentLineThickness(2.0f);
        jumpRemainGaugeBar->SetProgress(1.0f);
        jumpRemainGaugeBar->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = jumpRemainGaugeBar->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(jumpRemainGaugeBasePosition_);
        }
        jumpRemainGaugeBar_ = jumpRemainGaugeBar.get();
        (void)ctx->AddObject2D(std::move(jumpRemainGaugeBar));

        auto clearFade = std::make_unique<Sprite>();
        clearFade->SetName("ClearFadeSprite");
        clearFade->SetUniqueBatchKey();
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
        clearText->SetFont("Assets/Application/Image/KaqookanV2.fnt");
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

        TextureManager::TextureHandle jumpTexture = TextureManager::GetTextureFromFileName("uiOperationJump.png");
        if (jumpTexture == TextureManager::kInvalidHandle) {
            jumpTexture = TextureManager::GetTextureFromAssetPath("Application/Image/uiOperationJump.png");
        }

        TextureManager::TextureHandle gravityTexture = TextureManager::GetTextureFromFileName("uiOperationGravity.png");
        if (gravityTexture == TextureManager::kInvalidHandle) {
            gravityTexture = TextureManager::GetTextureFromAssetPath("Application/Image/uiOperationGravity.png");
        }

        TextureManager::TextureHandle gravityDirectionAllowTexture = TextureManager::GetTextureFromFileName("gravityChangeDirectionAllow.png");
        if (gravityDirectionAllowTexture == TextureManager::kInvalidHandle) {
            gravityDirectionAllowTexture = TextureManager::GetTextureFromAssetPath("Application/Image/gravityChangeDirectionAllow.png");
        }

        TextureManager::TextureHandle fastFallTexture = TextureManager::GetTextureFromFileName("uiOperationFastFalling.png");
        if (fastFallTexture == TextureManager::kInvalidHandle) {
            fastFallTexture = TextureManager::GetTextureFromAssetPath("Application/Image/uiOperationFastFalling.png");
        }

        float jumpWidth = operationUIFallbackSize_.x;
        float jumpHeight = operationUIFallbackSize_.y;
        if (jumpTexture != TextureManager::kInvalidHandle) {
            const auto view = TextureManager::GetTextureView(jumpTexture);
            jumpWidth = std::max(1.0f, static_cast<float>(view.GetWidth()));
            jumpHeight = std::max(1.0f, static_cast<float>(view.GetHeight()));
        }

        float gravityWidth = operationUIFallbackSize_.x;
        float gravityHeight = operationUIFallbackSize_.y;
        if (gravityTexture != TextureManager::kInvalidHandle) {
            const auto view = TextureManager::GetTextureView(gravityTexture);
            gravityWidth = std::max(1.0f, static_cast<float>(view.GetWidth()));
            gravityHeight = std::max(1.0f, static_cast<float>(view.GetHeight()));
        }

        float gravityDirectionAllowWidth = gravityDirectionAllowFallbackSize_.x;
        float gravityDirectionAllowHeight = gravityDirectionAllowFallbackSize_.y;
        if (gravityDirectionAllowTexture != TextureManager::kInvalidHandle) {
            const auto view = TextureManager::GetTextureView(gravityDirectionAllowTexture);
            gravityDirectionAllowWidth = std::max(1.0f, static_cast<float>(view.GetWidth()));
            gravityDirectionAllowHeight = std::max(1.0f, static_cast<float>(view.GetHeight()));
        }

        float fastFallWidth = operationUIFallbackSize_.x;
        float fastFallHeight = operationUIFallbackSize_.y;
        if (fastFallTexture != TextureManager::kInvalidHandle) {
            const auto view = TextureManager::GetTextureView(fastFallTexture);
            fastFallWidth = std::max(1.0f, static_cast<float>(view.GetWidth()));
            fastFallHeight = std::max(1.0f, static_cast<float>(view.GetHeight()));
        }

        const float operationUIBaseY = operationUIMarginBottom_ + operationUIStackOffsetY_;

        auto fastFallUI = std::make_unique<Sprite>();
        fastFallUI->SetName("OperationFastFallUISprite");
        fastFallUI->SetUniqueBatchKey();
        fastFallUI->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *mat = fastFallUI->GetComponent2D<Material2D>()) {
            mat->SetTexture(fastFallTexture);
            mat->SetColor(operationUIBaseColor_);
        }
        if (auto *tr = fastFallUI->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector3{fastFallWidth, fastFallHeight, 1.0f});
            const Vector3 basePos{
                screenWidth_ - operationUIMarginRight_ - fastFallWidth * 0.5f,
                operationUIBaseY + fastFallHeight * 0.5f,
                0.0f};
            tr->SetTranslate(basePos);
            operationFastFallUIBasePosition_ = basePos;
        }
        operationFastFallUISprite_ = fastFallUI.get();
        (void)ctx->AddObject2D(std::move(fastFallUI));

        auto gravityUI = std::make_unique<Sprite>();
        gravityUI->SetName("OperationGravityUISprite");
        gravityUI->SetUniqueBatchKey();
        gravityUI->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *mat = gravityUI->GetComponent2D<Material2D>()) {
            mat->SetTexture(gravityTexture);
            mat->SetColor(operationUIBaseColor_);
        }
        if (auto *tr = gravityUI->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector3{gravityWidth, gravityHeight, 1.0f});
            const Vector3 basePos{
                screenWidth_ - operationUIMarginRight_ - gravityWidth * 0.5f,
                operationUIBaseY + fastFallHeight + operationUIVerticalSpacing_ + gravityHeight * 0.5f,
                0.0f};
            tr->SetTranslate(basePos);
            operationGravityUIBasePosition_ = basePos;
        }
        operationGravityUISprite_ = gravityUI.get();
        (void)ctx->AddObject2D(std::move(gravityUI));

        auto jumpUI = std::make_unique<Sprite>();
        jumpUI->SetName("OperationJumpUISprite");
        jumpUI->SetUniqueBatchKey();
        jumpUI->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *mat = jumpUI->GetComponent2D<Material2D>()) {
            mat->SetTexture(jumpTexture);
            mat->SetColor(operationUIBaseColor_);
        }
        if (auto *tr = jumpUI->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector3{jumpWidth, jumpHeight, 1.0f});
            const Vector3 basePos{
                screenWidth_ - operationUIMarginRight_ - jumpWidth * 0.5f,
                operationUIBaseY + fastFallHeight + operationUIVerticalSpacing_ + gravityHeight + operationUIVerticalSpacing_ + jumpHeight * 0.5f,
                0.0f};
            tr->SetTranslate(basePos);
            operationJumpUIBasePosition_ = basePos;
        }
        operationJumpUISprite_ = jumpUI.get();
        (void)ctx->AddObject2D(std::move(jumpUI));

        auto gravityDirectionAllowUI = std::make_unique<Sprite>();
        gravityDirectionAllowUI->SetName("GravityChangeDirectionAllowUISprite");
        gravityDirectionAllowUI->SetUniqueBatchKey();
        gravityDirectionAllowUI->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *mat = gravityDirectionAllowUI->GetComponent2D<Material2D>()) {
            mat->SetTexture(gravityDirectionAllowTexture);
            Vector4 color = gravityDirectionAllowBaseColor_;
            color.w = 0.0f;
            mat->SetColor(color);
        }
        if (auto *tr = gravityDirectionAllowUI->GetComponent2D<Transform2D>()) {
            tr->SetScale(Vector3{gravityDirectionAllowWidth, gravityDirectionAllowHeight, 1.0f});
            const Vector3 basePos{
                operationGravityUIBasePosition_.x - (gravityWidth * 0.5f + gravityDirectionAllowWidth * 0.5f + gravityDirectionAllowMargin_),
                operationGravityUIBasePosition_.y,
                0.0f};
            tr->SetTranslate(basePos);
            gravityDirectionAllowUIBasePosition_ = basePos;
        }
        gravityDirectionAllowUISprite_ = gravityDirectionAllowUI.get();
        (void)ctx->AddObject2D(std::move(gravityDirectionAllowUI));

        ApplyVisibility();
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!isVisible_) {
            return;
        }

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (player_ && !playerMovementController_) {
            playerMovementController_ = player_->GetComponent3D<PlayerMovementController>();
        }
        if (player_ && !playerInputHandler_) {
            playerInputHandler_ = player_->GetComponent3D<PlayerInputHandler>();
        }
        if (!stageGroundGenerator_) {
            stageGroundGenerator_ = ctx->GetComponent<StageGroundGenerator>();
        }
        if (!stageGoalPlaneController_) {
            stageGoalPlaneController_ = ctx->GetComponent<StageGoalPlaneController>();
        }

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        bool jumpTriggered = false;
        bool gravitySwitchTriggered = false;
        bool fastFallTriggered = false;

        auto *ic = ctx->GetInputCommand();
        operationJumpUIActive_ = !(playerInputHandler_ && playerInputHandler_->IsGravitySwitching());
        operationGravityUIActive_ = !playerMovementController_ || playerMovementController_->CanUseGravityChange();
        if (playerMovementController_) {
            const Vector3 down = playerMovementController_->GetGravityDirection().Normalize();
            const float fallSpeed = playerMovementController_->GetGravityVelocity().Dot(down);
            operationFastFallUIActive_ = fallSpeed > 0.0f;
        } else {
            operationFastFallUIActive_ = false;
        }

        if (ic) {
            jumpTriggered = ic->Evaluate("PlayerJump").Triggered();
            gravitySwitchTriggered = ic->Evaluate("PlayerGravitySwitchTrigger").Triggered();
            fastFallTriggered = ic->Evaluate("PlayerForwardSpeedDown").Triggered();
            if (playerMovementController_ && gravitySwitchTriggered && !playerMovementController_->CanUseGravityChange()) {
                gravityGaugeShakeActive_ = true;
                gravityGaugeShakeElapsed_ = 0.0f;
            }

            UpdateOperationInputUISprite(operationJumpUISprite_, operationJumpUIBasePosition_, jumpTriggered, operationJumpUIActive_, operationJumpUIPressed_, operationJumpUIReleaseElapsed_, dt);
            UpdateOperationInputUISprite(operationGravityUISprite_, operationGravityUIBasePosition_, gravitySwitchTriggered, operationGravityUIActive_, operationGravityUIPressed_, operationGravityUIReleaseElapsed_, dt);
            UpdateOperationInputUISprite(operationFastFallUISprite_, operationFastFallUIBasePosition_, fastFallTriggered, operationFastFallUIActive_, operationFastFallUIPressed_, operationFastFallUIReleaseElapsed_, dt);
        } else {
            UpdateOperationInputUISprite(operationJumpUISprite_, operationJumpUIBasePosition_, false, operationJumpUIActive_, operationJumpUIPressed_, operationJumpUIReleaseElapsed_, dt);
            UpdateOperationInputUISprite(operationGravityUISprite_, operationGravityUIBasePosition_, false, operationGravityUIActive_, operationGravityUIPressed_, operationGravityUIReleaseElapsed_, dt);
            UpdateOperationInputUISprite(operationFastFallUISprite_, operationFastFallUIBasePosition_, false, operationFastFallUIActive_, operationFastFallUIPressed_, operationFastFallUIReleaseElapsed_, dt);
        }

        int jumpCount = 0;
        int maxJumpCount = 0;
        int remainingJumpCount = 0;
        float remainingJumpProgress = 0.0f;
        if (playerMovementController_) {
            jumpCount = std::max(0, playerMovementController_->GetJumpCount());
            maxJumpCount = std::max(0, playerMovementController_->GetMaxJumpCount());
            remainingJumpCount = std::max(0, maxJumpCount - jumpCount);
            remainingJumpProgress = (maxJumpCount > 0) ? std::clamp(static_cast<float>(remainingJumpCount) / static_cast<float>(maxJumpCount), 0.0f, 1.0f) : 0.0f;

            if (previousJumpCount_ < 0) {
                previousJumpCount_ = jumpCount;
                previousMaxJumpCount_ = maxJumpCount;
            }

            const bool jumpConsumedThisFrame = jumpCount > previousJumpCount_;
            if (jumpConsumedThisFrame) {
                jumpRemainGaugeAnimActive_ = true;
                jumpRemainGaugeAnimElapsed_ = 0.0f;
                jumpRemainGaugeAnimDuration_ = std::max(0.0001f, playerMovementController_->GetMaxJumpInputHoldTime());
                jumpRemainGaugeAnimStartProgress_ = std::clamp(jumpRemainGaugeDisplayProgress_, 0.0f, 1.0f);
                jumpRemainGaugeAnimTargetProgress_ = remainingJumpProgress;
                jumpRemainGaugeVisible_ = true;
                jumpRemainGaugeFadeOutActive_ = false;
                jumpRemainGaugeFadeOutElapsed_ = 0.0f;
                jumpRemainGaugeAlpha_ = 1.0f;
            } else if (!jumpRemainGaugeAnimActive_) {
                jumpRemainGaugeDisplayProgress_ = remainingJumpProgress;
            }

            if (jumpTriggered && remainingJumpCount <= 0) {
                jumpRemainGaugeVisible_ = true;
                jumpRemainGaugeFadeOutActive_ = false;
                jumpRemainGaugeFadeOutElapsed_ = 0.0f;
                jumpRemainGaugeAlpha_ = 1.0f;
                jumpRemainGaugeShakeActive_ = true;
                jumpRemainGaugeShakeElapsed_ = 0.0f;
            }

            if (jumpRemainGaugeVisible_) {
                if (remainingJumpCount >= maxJumpCount && maxJumpCount > 0) {
                    if (!jumpRemainGaugeFadeOutActive_) {
                        jumpRemainGaugeFadeOutActive_ = true;
                        jumpRemainGaugeFadeOutElapsed_ = 0.0f;
                    }
                } else {
                    jumpRemainGaugeFadeOutActive_ = false;
                    jumpRemainGaugeFadeOutElapsed_ = 0.0f;
                    jumpRemainGaugeAlpha_ = 1.0f;
                }

                if (jumpRemainGaugeFadeOutActive_) {
                    jumpRemainGaugeFadeOutElapsed_ += dt;
                    const float t = std::clamp(jumpRemainGaugeFadeOutElapsed_ / std::max(0.0001f, jumpRemainGaugeFadeOutDuration_), 0.0f, 1.0f);
                    jumpRemainGaugeAlpha_ = 1.0f - t;
                    if (t >= 1.0f) {
                        jumpRemainGaugeVisible_ = false;
                        jumpRemainGaugeFadeOutActive_ = false;
                        jumpRemainGaugeFadeOutElapsed_ = 0.0f;
                        jumpRemainGaugeAlpha_ = 0.0f;
                        jumpRemainGaugeShakeActive_ = false;
                        jumpRemainGaugeShakeElapsed_ = 0.0f;
                    }
                }
            }

            if (jumpRemainGaugeAnimActive_) {
                if (!jumpTriggered) {
                    jumpRemainGaugeAnimActive_ = false;
                    jumpRemainGaugeAnimElapsed_ = 0.0f;
                    jumpRemainGaugeDisplayProgress_ = std::clamp(jumpRemainGaugeAnimTargetProgress_, 0.0f, 1.0f);
                }
            }

            if (jumpRemainGaugeAnimActive_) {
                jumpRemainGaugeAnimElapsed_ += dt;
                const float t = std::clamp(jumpRemainGaugeAnimElapsed_ / std::max(0.0001f, jumpRemainGaugeAnimDuration_), 0.0f, 1.0f);
                const float eased = 1.0f - std::pow(1.0f - t, 3.0f);
                jumpRemainGaugeDisplayProgress_ = std::clamp(
                    jumpRemainGaugeAnimStartProgress_ + (jumpRemainGaugeAnimTargetProgress_ - jumpRemainGaugeAnimStartProgress_) * eased,
                    0.0f,
                    1.0f);
                if (t >= 1.0f) {
                    jumpRemainGaugeAnimActive_ = false;
                    jumpRemainGaugeAnimElapsed_ = 0.0f;
                    jumpRemainGaugeDisplayProgress_ = std::clamp(jumpRemainGaugeAnimTargetProgress_, 0.0f, 1.0f);
                }
            }

            previousJumpCount_ = jumpCount;
            previousMaxJumpCount_ = maxJumpCount;
        } else {
            previousJumpCount_ = -1;
            previousMaxJumpCount_ = -1;
            jumpRemainGaugeVisible_ = false;
            jumpRemainGaugeFadeOutActive_ = false;
            jumpRemainGaugeFadeOutElapsed_ = 0.0f;
            jumpRemainGaugeAlpha_ = 0.0f;
            jumpRemainGaugeDisplayProgress_ = 0.0f;
            jumpRemainGaugeAnimActive_ = false;
            jumpRemainGaugeAnimElapsed_ = 0.0f;
            jumpRemainGaugeShakeActive_ = false;
            jumpRemainGaugeShakeElapsed_ = 0.0f;
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

        if (gravityGaugeBar_) {
            if (auto *tr = gravityGaugeBar_->GetComponent2D<Transform2D>()) {
                float shakeOffsetX = 0.0f;
                if (gravityGaugeShakeActive_) {
                    gravityGaugeShakeElapsed_ += dt;
                    const float t = std::clamp(gravityGaugeShakeElapsed_ / std::max(0.0001f, gravityGaugeShakeDuration_), 0.0f, 1.0f);
                    if (t >= 1.0f) {
                        gravityGaugeShakeActive_ = false;
                    } else {
                        shakeOffsetX = std::sin(gravityGaugeShakeElapsed_ * gravityGaugeShakeAngularSpeed_) * gravityGaugeShakeAmplitude_ * (1.0f - t);
                    }
                }

                auto pos = gravityGaugeBarBasePosition_;
                pos.x += shakeOffsetX;
                tr->SetTranslate(pos);
            }
        }

        if (goalDistanceBar_ && player_ && stageGoalPlaneController_) {
            if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
                const float playerZ = playerTr->GetTranslate().z;
                const float goalZ = stageGoalPlaneController_->GetGoalZ();

                if (!goalDistanceInitialized_) {
                    goalDistanceStartZ_ = playerZ;
                    const float denom = goalZ - goalDistanceStartZ_;
                    goalDistanceProgressDenominator_ = (std::abs(denom) > 0.0001f) ? denom : ((denom >= 0.0f) ? 0.0001f : -0.0001f);
                    goalDistanceInitialized_ = true;
                }

                const float rawProgress = (playerZ - goalDistanceStartZ_) / goalDistanceProgressDenominator_;
                goalDistanceBar_->SetProgress(std::clamp(rawProgress, 0.0f, 1.0f));
            }
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

        if (jumpRemainGaugeBar_) {
            jumpRemainGaugeBar_->SetProgress(std::clamp(jumpRemainGaugeDisplayProgress_, 0.0f, 1.0f));
            jumpRemainGaugeBar_->SetSegmentLineCount(std::max(0, maxJumpCount - 1));

            if (auto *tr = jumpRemainGaugeBar_->GetComponent2D<Transform2D>()) {
                Vector3 pos = jumpRemainGaugeBasePosition_;
                if (player_) {
                    if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
                        Vector2 playerScreen;
                        if (ProjectWorldTo2DWorld(playerTr->GetTranslate(), playerScreen)) {
                            pos = Vector3{playerScreen.x, playerScreen.y + jumpRemainGaugeYOffset_, 0.0f};
                        }
                    }
                }

                float shakeOffsetX = 0.0f;
                if (jumpRemainGaugeShakeActive_) {
                    jumpRemainGaugeShakeElapsed_ += dt;
                    const float t = std::clamp(jumpRemainGaugeShakeElapsed_ / std::max(0.0001f, jumpRemainGaugeShakeDuration_), 0.0f, 1.0f);
                    if (t >= 1.0f) {
                        jumpRemainGaugeShakeActive_ = false;
                        jumpRemainGaugeShakeElapsed_ = 0.0f;
                    } else {
                        shakeOffsetX = std::sin(jumpRemainGaugeShakeElapsed_ * jumpRemainGaugeShakeAngularSpeed_) * jumpRemainGaugeShakeAmplitude_ * (1.0f - t);
                    }
                }

                pos.x += shakeOffsetX;
                tr->SetTranslate(pos);
            }

            const float alpha = (jumpRemainGaugeVisible_ && isVisible_) ? std::clamp(jumpRemainGaugeAlpha_, 0.0f, 1.0f) : 0.0f;
            auto applyJumpGaugeColor = [&](const Vector4 &base, auto setter) {
                Vector4 c = base;
                c.w *= alpha;
                (jumpRemainGaugeBar_->*setter)(c);
            };
            applyJumpGaugeColor(jumpRemainFrameColorBase_, &SpriteProressBar::SetFrameColor);
            applyJumpGaugeColor(jumpRemainBackgroundColorBase_, &SpriteProressBar::SetBackgroundColor);
            applyJumpGaugeColor(jumpRemainBarColorBase_, &SpriteProressBar::SetBarColor);
            applyJumpGaugeColor(jumpRemainSegmentColorBase_, &SpriteProressBar::SetSegmentLineColor);
        }

        UpdateGravityDirectionAllowUI();

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
                    if (auto *ic2 = ctx->GetInputCommand()) {
                        if (ic2->Evaluate("Submit").Triggered()) {
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
    void ApplyVisibility() {
        const float alpha = isVisible_ ? 1.0f : 0.0f;

        auto applyBarAlpha = [&](SpriteProressBar *bar, const Vector4 &frameBase, const Vector4 &bgBase, const Vector4 &fillBase, const Vector4 &segmentBase) {
            if (!bar) return;
            Vector4 c = frameBase;
            c.w *= alpha;
            bar->SetFrameColor(c);
            c = bgBase;
            c.w *= alpha;
            bar->SetBackgroundColor(c);
            c = fillBase;
            c.w *= alpha;
            bar->SetBarColor(c);
            c = segmentBase;
            c.w *= alpha;
            bar->SetSegmentLineColor(c);
        };

        applyBarAlpha(forwardSpeedBar_, forwardFrameColorBase_, forwardBackgroundColorBase_, forwardBarColorBase_, forwardSegmentColorBase_);
        applyBarAlpha(gravityGaugeBar_, gravityFrameColorBase_, gravityBackgroundColorBase_, gravityBarColorBase_, gravitySegmentColorBase_);
        applyBarAlpha(goalDistanceBar_, goalFrameColorBase_, goalBackgroundColorBase_, goalBarColorBase_, goalSegmentColorBase_);
        applyBarAlpha(jumpRemainGaugeBar_, jumpRemainFrameColorBase_, jumpRemainBackgroundColorBase_, jumpRemainBarColorBase_, jumpRemainSegmentColorBase_);

        if (!isVisible_ && gravityGaugeBar_) {
            if (auto *tr = gravityGaugeBar_->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(gravityGaugeBarBasePosition_);
            }
            gravityGaugeShakeActive_ = false;
            gravityGaugeShakeElapsed_ = 0.0f;
        }

        if (!isVisible_ && jumpRemainGaugeBar_) {
            if (auto *tr = jumpRemainGaugeBar_->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(jumpRemainGaugeBasePosition_);
            }
            jumpRemainGaugeShakeActive_ = false;
            jumpRemainGaugeShakeElapsed_ = 0.0f;
        }

        SetTextAlpha(forwardSpeedText_, alpha);
        SetTextAlpha(touchedGroundCountText_, alpha);
        SetTextAlpha(fallDistanceText_, alpha);
        SetTextAlpha(landingTouchedGroundCountText_, alpha);

        if (clearFadeSprite_) {
            if (auto *mat = clearFadeSprite_->GetComponent2D<Material2D>()) {
                auto color = mat->GetColor();
                color.w = isVisible_ ? color.w : 0.0f;
                mat->SetColor(color);
            }
        }

        if (operationJumpUISprite_) {
            if (auto *mat = operationJumpUISprite_->GetComponent2D<Material2D>()) {
                Vector4 color = operationUIBaseColor_;
                color.w *= alpha;
                mat->SetColor(color);
            }
            if (!isVisible_) {
                if (auto *tr = operationJumpUISprite_->GetComponent2D<Transform2D>()) {
                    tr->SetTranslate(operationJumpUIBasePosition_);
                }
                operationJumpUIPressed_ = false;
                operationJumpUIReleaseElapsed_ = 0.0f;
            }
        }

        if (operationGravityUISprite_) {
            if (auto *mat = operationGravityUISprite_->GetComponent2D<Material2D>()) {
                Vector4 color = operationUIBaseColor_;
                color.w *= alpha;
                mat->SetColor(color);
            }
            if (!isVisible_) {
                if (auto *tr = operationGravityUISprite_->GetComponent2D<Transform2D>()) {
                    tr->SetTranslate(operationGravityUIBasePosition_);
                }
                operationGravityUIPressed_ = false;
                operationGravityUIReleaseElapsed_ = 0.0f;
            }
        }

        if (operationFastFallUISprite_) {
            if (auto *mat = operationFastFallUISprite_->GetComponent2D<Material2D>()) {
                Vector4 color = operationUIBaseColor_;
                color.w *= alpha;
                mat->SetColor(color);
            }
            if (!isVisible_) {
                if (auto *tr = operationFastFallUISprite_->GetComponent2D<Transform2D>()) {
                    tr->SetTranslate(operationFastFallUIBasePosition_);
                }
                operationFastFallUIPressed_ = false;
                operationFastFallUIReleaseElapsed_ = 0.0f;
            }
        }

        if (gravityDirectionAllowUISprite_) {
            if (auto *mat = gravityDirectionAllowUISprite_->GetComponent2D<Material2D>()) {
                Vector4 color = gravityDirectionAllowBaseColor_;
                color.w *= 0.0f;
                mat->SetColor(color);
            }
            if (auto *tr = gravityDirectionAllowUISprite_->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(gravityDirectionAllowUIBasePosition_);
                tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
            }
        }

        if (!isVisible_ && clearResultText_) {
            clearResultText_->SetText(" ");
            SetTextAlpha(clearResultText_, 0.0f);
        }
    }

    void UpdateOperationInputUISprite(
        Sprite *sprite,
        const Vector3 &basePosition,
        bool inputTriggered,
        bool isActive,
        bool &pressedState,
        float &releaseElapsed,
        float dt) {
        if (!sprite) return;

        auto *tr = sprite->GetComponent2D<Transform2D>();
        auto *mat = sprite->GetComponent2D<Material2D>();
        if (!tr || !mat) return;

        if (!isActive) {
            pressedState = false;
            releaseElapsed = 0.0f;
            tr->SetTranslate(basePosition);
            Vector4 color = operationUIInactiveColor_;
            color.w *= (isVisible_ ? 1.0f : 0.0f);
            mat->SetColor(color);
            return;
        }

        if (inputTriggered) {
            pressedState = true;
            releaseElapsed = 0.0f;
            Vector3 pos = basePosition;
            pos.x += operationUIPressedOffsetX_;
            tr->SetTranslate(pos);

            Vector4 color = operationUIActiveColor_;
            color.w *= (isVisible_ ? 1.0f : 0.0f);
            mat->SetColor(color);
            return;
        }

        Vector3 pos = basePosition;
        if (pressedState) {
            releaseElapsed += dt;
            const float t = std::clamp(releaseElapsed / std::max(0.0001f, operationUIReleaseDuration_), 0.0f, 1.0f);
            const float eased = 1.0f - std::pow(1.0f - t, 3.0f);
            pos.x += operationUIPressedOffsetX_ * (1.0f - eased);
            if (t >= 1.0f) {
                pressedState = false;
            }
        }
        tr->SetTranslate(pos);

        Vector4 color = operationUIBaseColor_;
        color.w *= (isVisible_ ? 1.0f : 0.0f);
        mat->SetColor(color);
    }

    void UpdateGravityDirectionAllowUI() {
        if (!gravityDirectionAllowUISprite_) return;

        auto *tr = gravityDirectionAllowUISprite_->GetComponent2D<Transform2D>();
        auto *mat = gravityDirectionAllowUISprite_->GetComponent2D<Material2D>();
        if (!tr || !mat) return;

        bool visible = false;
        Vector3 direction{0.0f, 1.0f, 0.0f};

        if (playerInputHandler_ && playerInputHandler_->IsGravitySwitching()) {
            visible = true;
            if (auto requested = playerInputHandler_->GetRequestedGravityDirection(); requested.has_value()) {
                direction = *requested;
            } else if (playerMovementController_) {
                direction = -playerMovementController_->GetGravityDirection();
            }
        }

        Vector4 color = gravityDirectionAllowBaseColor_;
        color.w *= (visible && isVisible_) ? 1.0f : 0.0f;
        mat->SetColor(color);

        if (!visible) {
            tr->SetTranslate(gravityDirectionAllowUIBasePosition_);
            tr->SetRotate(Vector3{0.0f, 0.0f, 0.0f});
            return;
        }

        Vector2 screenDirection;
        if (!GetGravityDirectionScreenVector(direction, screenDirection)) {
            screenDirection = Vector2{0.0f, 1.0f};
        }

        Vector3 uiPosition = gravityDirectionAllowUIBasePosition_;
        if (player_) {
            if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
                Vector2 playerScreen;
                if (ProjectWorldTo2DWorld(playerTr->GetTranslate(), playerScreen)) {
                    uiPosition = Vector3{
                        playerScreen.x + screenDirection.x * gravityDirectionAllowRadius_,
                        playerScreen.y + screenDirection.y * gravityDirectionAllowRadius_,
                        0.0f};
                }
            }
        }

        const float angle = GetGravityDirectionArrowAngleFromScreenDirection(screenDirection);
        tr->SetTranslate(uiPosition);
        tr->SetRotate(Vector3{0.0f, 0.0f, angle});
    }

    bool GetGravityDirectionScreenVector(const Vector3 &direction, Vector2 &outScreenDirection) const {
        outScreenDirection = Vector2{direction.x, direction.y};

        if (player_ && mainCamera_) {
            if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
                const Vector3 base = playerTr->GetTranslate();
                const Vector3 dir = direction.Normalize();
                Vector2 p0;
                Vector2 p1;
                if (ProjectWorldTo2DWorld(base, p0) && ProjectWorldTo2DWorld(base + dir * 5.0f, p1)) {
                    outScreenDirection = p1 - p0;
                }
            }
        }

        const float lenSq = outScreenDirection.x * outScreenDirection.x + outScreenDirection.y * outScreenDirection.y;
        if (lenSq <= 0.000001f) {
            return false;
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        outScreenDirection.x *= invLen;
        outScreenDirection.y *= invLen;
        return true;
    }

    float GetGravityDirectionArrowAngleFromScreenDirection(const Vector2 &screenDirection) const {
        return std::atan2(-screenDirection.x, screenDirection.y);
    }

    float GetGravityDirectionArrowAngle(const Vector3 &direction) const {
        Vector2 screenDirection;
        if (!GetGravityDirectionScreenVector(direction, screenDirection)) {
            return 0.0f;
        }
        return GetGravityDirectionArrowAngleFromScreenDirection(screenDirection);
    }

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
    PlayerInputHandler *playerInputHandler_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;
    StageGoalPlaneController *stageGoalPlaneController_ = nullptr;
    Camera3D *mainCamera_ = nullptr;

    SpriteProressBar *forwardSpeedBar_ = nullptr;
    SpriteProressBar *gravityGaugeBar_ = nullptr;
    SpriteProressBar *goalDistanceBar_ = nullptr;
    SpriteProressBar *jumpRemainGaugeBar_ = nullptr;
    Text *forwardSpeedText_ = nullptr;
    Text *touchedGroundCountText_ = nullptr;
    Text *fallDistanceText_ = nullptr;
    Text *landingTouchedGroundCountText_ = nullptr;
    Sprite *clearFadeSprite_ = nullptr;
    Text *clearResultText_ = nullptr;
    Sprite *operationJumpUISprite_ = nullptr;
    Sprite *operationGravityUISprite_ = nullptr;
    Sprite *gravityDirectionAllowUISprite_ = nullptr;
    Sprite *operationFastFallUISprite_ = nullptr;

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

    bool goalDistanceInitialized_ = false;
    float goalDistanceStartZ_ = 0.0f;
    float goalDistanceProgressDenominator_ = -1.0f;

    Vector3 gravityGaugeBarBasePosition_{320.0f, 70.0f, 0.0f};
    bool gravityGaugeShakeActive_ = false;
    float gravityGaugeShakeElapsed_ = 0.0f;
    float gravityGaugeShakeDuration_ = 0.25f;
    float gravityGaugeShakeAmplitude_ = 20.0f;
    float gravityGaugeShakeAngularSpeed_ = 80.0f;

    bool isVisible_ = true;

    Vector4 forwardFrameColorBase_{0.5f, 0.5f, 0.5f, 1.0f};
    Vector4 forwardBackgroundColorBase_{0.1f, 0.1f, 0.1f, 1.0f};
    Vector4 forwardBarColorBase_{0.0f, 0.5f, 0.0f, 1.0f};
    Vector4 forwardSegmentColorBase_{1.0f, 1.0f, 1.0f, 0.6f};

    Vector4 gravityFrameColorBase_{0.35f, 0.35f, 0.55f, 1.0f};
    Vector4 gravityBackgroundColorBase_{0.08f, 0.08f, 0.1f, 1.0f};
    Vector4 gravityBarColorBase_{0.2f, 0.6f, 1.0f, 1.0f};
    Vector4 gravitySegmentColorBase_{0.5f, 0.5f, 0.5f, 1.0f};

    Vector4 goalFrameColorBase_{0.4f, 0.4f, 0.4f, 1.0f};
    Vector4 goalBackgroundColorBase_{0.08f, 0.08f, 0.08f, 1.0f};
    Vector4 goalBarColorBase_{0.95f, 0.85f, 0.2f, 1.0f};
    Vector4 goalSegmentColorBase_{0.6f, 0.6f, 0.6f, 1.0f};

    Vector4 jumpRemainFrameColorBase_{0.75f, 0.75f, 0.75f, 1.0f};
    Vector4 jumpRemainBackgroundColorBase_{0.1f, 0.1f, 0.12f, 1.0f};
    Vector4 jumpRemainBarColorBase_{0.45f, 0.95f, 0.45f, 1.0f};
    Vector4 jumpRemainSegmentColorBase_{0.85f, 0.85f, 0.85f, 1.0f};

    Vector3 jumpRemainGaugeBasePosition_{0.0f, 0.0f, 0.0f};
    float jumpRemainGaugeYOffset_ = 176.0f;
    bool jumpRemainGaugeVisible_ = false;
    float jumpRemainGaugeAlpha_ = 0.0f;
    bool jumpRemainGaugeFadeOutActive_ = false;
    float jumpRemainGaugeFadeOutElapsed_ = 0.0f;
    float jumpRemainGaugeFadeOutDuration_ = 1.0f;
    float jumpRemainGaugeDisplayProgress_ = 0.0f;
    bool jumpRemainGaugeAnimActive_ = false;
    float jumpRemainGaugeAnimElapsed_ = 0.0f;
    float jumpRemainGaugeAnimDuration_ = 0.5f;
    float jumpRemainGaugeAnimStartProgress_ = 0.0f;
    float jumpRemainGaugeAnimTargetProgress_ = 0.0f;
    int previousJumpCount_ = -1;
    int previousMaxJumpCount_ = -1;
    bool jumpRemainGaugeShakeActive_ = false;
    float jumpRemainGaugeShakeElapsed_ = 0.0f;
    float jumpRemainGaugeShakeDuration_ = 0.25f;
    float jumpRemainGaugeShakeAmplitude_ = 16.0f;
    float jumpRemainGaugeShakeAngularSpeed_ = 80.0f;

    Vector2 operationUIFallbackSize_{512.0f, 256.0f};
    Vector2 gravityDirectionAllowFallbackSize_{96.0f, 96.0f};
    float operationUIMarginRight_ = 32.0f;
    float operationUIMarginBottom_ = 24.0f;
    float operationUIVerticalSpacing_ = 12.0f;
    float operationUIStackOffsetY_ = 96.0f;
    float gravityDirectionAllowMargin_ = 24.0f;
    float operationUIPressedOffsetX_ = -128.0f;
    float operationUIReleaseDuration_ = 0.2f;
    Vector3 operationJumpUIBasePosition_{0.0f, 0.0f, 0.0f};
    Vector3 operationGravityUIBasePosition_{0.0f, 0.0f, 0.0f};
    Vector3 operationFastFallUIBasePosition_{0.0f, 0.0f, 0.0f};
    Vector3 gravityDirectionAllowUIBasePosition_{0.0f, 0.0f, 0.0f};
    bool operationJumpUIPressed_ = false;
    bool operationGravityUIPressed_ = false;
    bool operationFastFallUIPressed_ = false;
    bool operationJumpUIActive_ = true;
    bool operationGravityUIActive_ = true;
    bool operationFastFallUIActive_ = false;
    float operationJumpUIReleaseElapsed_ = 0.0f;
    float operationGravityUIReleaseElapsed_ = 0.0f;
    float operationFastFallUIReleaseElapsed_ = 0.0f;
    Vector4 operationUIBaseColor_{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 operationUIActiveColor_{1.0f, 1.0f, 0.0f, 1.0f};
    Vector4 operationUIInactiveColor_{0.5f, 0.5f, 0.5f, 1.0f};
    Vector4 gravityDirectionAllowBaseColor_{1.0f, 1.0f, 1.0f, 1.0f};
    float gravityDirectionAllowRadius_ = 128.0f;
};

} // namespace KashipanEngine
