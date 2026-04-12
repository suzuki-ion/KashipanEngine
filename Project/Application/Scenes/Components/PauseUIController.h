#pragma once

#include <KashipanEngine.h>

#include "Scenes/Components/SceneChangeOut.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>

namespace KashipanEngine {

class PauseUIController final : public ISceneComponent {
public:
    enum class RequestAction {
        None,
        Continue,
        Retry,
        BackToTitle,
        Quit,
    };

    PauseUIController()
        : ISceneComponent("PauseUIController", 1) {}

    ~PauseUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        sceneDefaultVariables_ = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
        if (!screenBuffer2D) return;

        const float screenW = static_cast<float>(screenBuffer2D->GetWidth());
        const float screenH = static_cast<float>(screenBuffer2D->GetHeight());
        const float cx = screenW * 0.5f;
        const float cy = screenH * 0.5f;

        auto bg = std::make_unique<Sprite>();
        bg->SetName("PauseBackground");
        bg->SetUniqueBatchKey();
        bg->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = bg->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy, 0.0f});
            tr->SetScale(Vector3{screenW, screenH, 1.0f});
        }
        if (auto *mat = bg->GetComponent2D<Material2D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("white1x1.png"));
            mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.0f});
        }
        backgroundSprite_ = bg.get();
        (void)ctx->AddObject2D(std::move(bg));

        auto logo = std::make_unique<Text>(64);
        logo->SetName("PauseLogoText");
        logo->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        logo->SetText(" ");
        logo->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        logo->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = logo->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy + 140.0f, 0.0f});
        }
        logoText_ = logo.get();
        (void)ctx->AddObject2D(std::move(logo));

        auto resume = std::make_unique<Text>(64);
        resume->SetName("PauseResumeText");
        resume->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        resume->SetText(" ");
        resume->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        resume->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = resume->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy + 40.0f, 0.0f});
        }
        resumeText_ = resume.get();
        (void)ctx->AddObject2D(std::move(resume));

        auto retry = std::make_unique<Text>(64);
        retry->SetName("PauseRetryText");
        retry->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        retry->SetText(" ");
        retry->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        retry->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = retry->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy - 20.0f, 0.0f});
        }
        retryText_ = retry.get();
        (void)ctx->AddObject2D(std::move(retry));

        auto backTitle = std::make_unique<Text>(64);
        backTitle->SetName("PauseBackToTitleText");
        backTitle->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        backTitle->SetText(" ");
        backTitle->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        backTitle->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = backTitle->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy - 80.0f, 0.0f});
        }
        backToTitleText_ = backTitle.get();
        (void)ctx->AddObject2D(std::move(backTitle));

        auto quit = std::make_unique<Text>(64);
        quit->SetName("PauseQuitText");
        quit->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        quit->SetText(" ");
        quit->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        quit->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = quit->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy - 140.0f, 0.0f});
        }
        quitText_ = quit.get();
        (void)ctx->AddObject2D(std::move(quit));

        cacheTextTransforms();
    }

    void Activate() {
        if (isActive_) return;
        isActive_ = true;
        selectionIndex_ = 0;
        introElapsed_ = 0.0f;
        requestedAction_ = RequestAction::None;
        resumeGameSpeed_ = GetGameSpeed();
        SetGameSpeed(0.0f);

        setTextAlpha(logoText_, 0.0f);
        setTextAlpha(resumeText_, 0.0f);
        setTextAlpha(retryText_, 0.0f);
        setTextAlpha(backToTitleText_, 0.0f);
        setTextAlpha(quitText_, 0.0f);
        if (backgroundSprite_) {
            if (auto *mat = backgroundSprite_->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.5f});
            }
        }

        RefreshTexts();
    }

    void Deactivate() {
        if (!isActive_) return;
        isActive_ = false;
        SetGameSpeed(resumeGameSpeed_);
        if (backgroundSprite_) {
            if (auto *mat = backgroundSprite_->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.0f});
            }
        }
        setTextAlpha(logoText_, 0.0f);
        setTextAlpha(resumeText_, 0.0f);
        setTextAlpha(retryText_, 0.0f);
        setTextAlpha(backToTitleText_, 0.0f);
        setTextAlpha(quitText_, 0.0f);
    }

    bool IsActive() const { return isActive_; }

    RequestAction ConsumeRequestedAction() {
        const RequestAction action = requestedAction_;
        requestedAction_ = RequestAction::None;
        return action;
    }

    void Update() override {
        if (!isActive_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *ic = ctx->GetInputCommand();
        if (!ic) return;

        introElapsed_ += std::max(0.0f, GetDeltaTime());
        updateEntranceAnimation();
        updateVerticalBobbing();

        if (ic->Evaluate("SelectUp").Triggered()) {
            selectionIndex_ = (selectionIndex_ + 3) % 4;
            RefreshTexts();
        }
        if (ic->Evaluate("SelectDown").Triggered()) {
            selectionIndex_ = (selectionIndex_ + 1) % 4;
            RefreshTexts();
        }

        if (!ic->Evaluate("Submit").Triggered()) return;

        if (selectionIndex_ == 0) {
            requestedAction_ = RequestAction::Continue;
            Deactivate();
            return;
        }

        if (selectionIndex_ == 1) {
            requestedAction_ = RequestAction::Retry;
        } else if (selectionIndex_ == 2) {
            requestedAction_ = RequestAction::BackToTitle;
        } else {
            requestedAction_ = RequestAction::Quit;
            if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
                sceneDefaultVariables_->GetMainWindow()->DestroyNotify();
            }
        }

        if (auto *out = ctx->GetComponent<SceneChangeOut>()) {
            out->Play();
        }
    }

private:
    void RefreshTexts() {
        if (logoText_) {
            logoText_->SetText("ポーズ");
        }
        if (resumeText_) {
            resumeText_->SetText(selectionIndex_ == 0 ? "＞ つづける" : "  つづける");
        }
        if (retryText_) {
            retryText_->SetText(selectionIndex_ == 1 ? "＞ やりなおす" : "  やりなおす");
        }
        if (backToTitleText_) {
            backToTitleText_->SetText(selectionIndex_ == 2 ? "＞ タイトルにもどる" : "  タイトルにもどる");
        }
        if (quitText_) {
            quitText_->SetText(selectionIndex_ == 3 ? "＞ おわる" : "  おわる");
        }
    }

    void cacheTextTransforms() {
        std::array<Text *, 5> texts = {logoText_, resumeText_, retryText_, backToTitleText_, quitText_};
        for (size_t i = 0; i < texts.size(); ++i) {
            if (!texts[i]) continue;
            auto *tr = texts[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;
            basePositions_[i] = tr->GetTranslate();
            startPositions_[i] = Vector3{basePositions_[i].x + introFromRightOffset_, basePositions_[i].y, basePositions_[i].z};
        }
    }

    void updateEntranceAnimation() {
        std::array<Text *, 5> texts = {logoText_, resumeText_, retryText_, backToTitleText_, quitText_};
        for (size_t i = 0; i < texts.size(); ++i) {
            if (!texts[i]) continue;
            auto *tr = texts[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;

            const float local = std::clamp((introElapsed_ - introDelaySec_ * static_cast<float>(i)) / introDurationSec_, 0.0f, 1.0f);
            const float x = EaseOutCubic(startPositions_[i].x, basePositions_[i].x, local);
            tr->SetTranslate(Vector3{x, tr->GetTranslate().y, basePositions_[i].z});
            setTextAlpha(texts[i], local);
        }
    }

    void updateVerticalBobbing() {
        const float t = introElapsed_ * bobSpeed_;
        auto apply = [&](Text *text, size_t index, bool enabled) {
            if (!text) return;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) return;
            auto pos = tr->GetTranslate();
            pos.y = basePositions_[index].y + (enabled ? std::sin(t) * bobAmplitude_ : 0.0f);
            tr->SetTranslate(pos);
        };

        apply(logoText_, 0, true);
        apply(resumeText_, 1, selectionIndex_ == 0);
        apply(retryText_, 2, selectionIndex_ == 1);
        apply(backToTitleText_, 3, selectionIndex_ == 2);
        apply(quitText_, 4, selectionIndex_ == 3);
    }

    void setTextAlpha(Text *text, float alpha) {
        if (!text) return;
        for (size_t i = 0; i < 64; ++i) {
            auto *sprite = (*text)[i];
            if (!sprite) continue;
            auto *mat = sprite->GetComponent2D<Material2D>();
            if (!mat) continue;
            auto color = mat->GetColor();
            color.w = std::clamp(alpha, 0.0f, 1.0f);
            mat->SetColor(color);
        }
    }

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

    Text *logoText_ = nullptr;
    Text *resumeText_ = nullptr;
    Text *retryText_ = nullptr;
    Text *backToTitleText_ = nullptr;
    Text *quitText_ = nullptr;
    Sprite *backgroundSprite_ = nullptr;

    bool isActive_ = false;
    int selectionIndex_ = 0;
    RequestAction requestedAction_ = RequestAction::None;
    float resumeGameSpeed_ = 1.0f;

    float introElapsed_ = 0.0f;
    float introDelaySec_ = 0.08f;
    float introDurationSec_ = 0.35f;
    float introFromRightOffset_ = 420.0f;
    float bobAmplitude_ = 6.0f;
    float bobSpeed_ = 6.0f;
    std::array<Vector3, 5> basePositions_{};
    std::array<Vector3, 5> startPositions_{};
};

} // namespace KashipanEngine
