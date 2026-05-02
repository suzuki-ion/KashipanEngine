#pragma once
#pragma once

#include <KashipanEngine.h>

#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/ClearScoreBoard.h"
#include "Scenes/Components/ClearTimeBoard.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <string>

namespace KashipanEngine {

class GameClearUIController final : public ISceneComponent {
public:
    enum class RequestAction {
        None,
        Retry,
        BackToTitle,
        Quit,
    };

    GameClearUIController(bool enableTouchGroundUi = false)
        : ISceneComponent("GameClearUIController", 1),
          isTouchGroundUiEnabled_(enableTouchGroundUi) {}

    ~GameClearUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        sceneDefaultVariables_ = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
        if (!screenBuffer2D) return;

        if (isTouchGroundUiEnabled_) {
            scoreboard_ = ctx->GetComponent<ClearScoreBoard>();
        }
        clearTimeBoard_ = ctx->GetComponent<ClearTimeBoard>();

        const float screenW = static_cast<float>(screenBuffer2D->GetWidth());
        const float screenH = static_cast<float>(screenBuffer2D->GetHeight());
        const float cx = screenW * 0.5f;
        const float cy = screenH * 0.5f;
        const float leftX = std::max(48.0f, screenW * 0.08f);
        const float logoY = cy + 392.0f;
        const float touchedY = logoY - 160.0f;
        const float rankingStartY = touchedY - 96.0f;
        const float retryY = cy - 168.0f;
        const float backY = retryY - 80.0f;
        const float quitY = backY - 80.0f;

        auto bg = std::make_unique<Sprite>();
        bg->SetName("GameClearBackground");
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

        auto logo = std::make_unique<Text>(128);
        logo->SetName("GameClearLogoText");
        logo->SetFont("Assets/Application/Image/KaqookanV2_Logo.fnt");
        logo->SetText(" ");
        logo->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
        logo->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = logo->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{leftX, logoY, 0.0f});
        }
        logoText_ = logo.get();
        (void)ctx->AddObject2D(std::move(logo));

        if (isTouchGroundUiEnabled_) {
            auto touched = std::make_unique<Text>(64);
            touched->SetName("GameClearTouchedGroundText");
            touched->SetFont("Assets/Application/Image/KaqookanV2.fnt");
            touched->SetText(" ");
            touched->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
            touched->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
            if (auto *tr = touched->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector3{leftX, touchedY, 0.0f});
            }
            touchedGroundText_ = touched.get();
            (void)ctx->AddObject2D(std::move(touched));
        }

        auto clearTime = std::make_unique<Text>(64);
        clearTime->SetName("GameClearClearTimeText");
        clearTime->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        clearTime->SetText(" ");
        clearTime->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
        clearTime->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = clearTime->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{leftX, touchedY, 0.0f});
        }
        clearTimeText_ = clearTime.get();
        (void)ctx->AddObject2D(std::move(clearTime));

        for (size_t i = 0; i < rankingTextLines_.size(); ++i) {
            auto rank = std::make_unique<Text>(64);
            rank->SetName("GameClearClearTimeRankingLine");
            rank->SetFont("Assets/Application/Image/KaqookanV2.fnt");
            rank->SetText(" ");
            rank->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
            rank->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
            if (auto *tr = rank->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector3{leftX, rankingStartY - static_cast<float>(i) * 56.0f, 0.0f});
            }
            rankingTextLines_[i] = rank.get();
            (void)ctx->AddObject2D(std::move(rank));
        }

        auto retry = std::make_unique<Text>(64);
        retry->SetName("GameClearRetryText");
        retry->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        retry->SetText(" ");
        retry->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
        retry->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = retry->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{leftX, retryY, 0.0f});
        }
        retryText_ = retry.get();
        (void)ctx->AddObject2D(std::move(retry));

        auto backTitle = std::make_unique<Text>(64);
        backTitle->SetName("GameClearBackToTitleText");
        backTitle->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        backTitle->SetText(" ");
        backTitle->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
        backTitle->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = backTitle->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{leftX, backY, 0.0f});
        }
        backToTitleText_ = backTitle.get();
        (void)ctx->AddObject2D(std::move(backTitle));

        auto quit = std::make_unique<Text>(64);
        quit->SetName("GameClearQuitText");
        quit->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        quit->SetText(" ");
        quit->SetTextAlign(TextAlignX::Left, TextAlignY::Center);
        quit->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = quit->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{leftX, quitY, 0.0f});
        }
        quitText_ = quit.get();
        (void)ctx->AddObject2D(std::move(quit));

        cacheTextTransforms();
    }

    void Activate(int touchedGroundCount) {
        auto *ctx = GetOwnerContext();
        if (!clearTimeBoard_ && ctx) {
            clearTimeBoard_ = ctx->GetComponent<ClearTimeBoard>();
        }

        isActive_ = true;
        selectionIndex_ = 0;
        previousSelectionIndex_ = 0;
        touchedGroundCount_ = touchedGroundCount;
        highlightedRankIndex_ = -1;
        clearTimeMilliseconds_ = 0;
        if (clearTimeBoard_) {
            clearTimeBoard_->PauseMeasurement();
            clearTimeMilliseconds_ = clearTimeBoard_->RegisterCurrentTime();
            const int rank = clearTimeBoard_->FindBestRank(clearTimeMilliseconds_);
            highlightedRankIndex_ = (rank > 0) ? (rank - 1) : -1;
        }
        introElapsed_ = 0.0f;
        setTextAlpha(logoText_, 0.0f);
        setTextAlpha(touchedGroundText_, 0.0f);
        setTextAlpha(clearTimeText_, 0.0f);
        setTextAlpha(retryText_, 0.0f);
        setTextAlpha(backToTitleText_, 0.0f);
        setTextAlpha(quitText_, 0.0f);
        if (backgroundSprite_) {
            if (auto *mat = backgroundSprite_->GetComponent2D<Material2D>()) {
                mat->SetColor(Vector4{0.0f, 0.0f, 0.0f, 0.5f});
            }
        }
        optionAnimElapsed_.fill(optionAnimDuration_);
        optionAnimElapsed_[0] = 0.0f;
        RefreshTexts();
        RefreshRankingTexts();
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
        updateLogoBobbing();
        updateOptionSelectionAnimation(std::max(0.0f, GetDeltaTime()));

        if (ic->Evaluate("SelectUp").Triggered()) {
            const int old = selectionIndex_;
            selectionIndex_ = (selectionIndex_ + 2) % 3;
            RefreshTexts();
            onSelectionChanged(old, selectionIndex_);
        }
        if (ic->Evaluate("SelectDown").Triggered()) {
            const int old = selectionIndex_;
            selectionIndex_ = (selectionIndex_ + 1) % 3;
            RefreshTexts();
            onSelectionChanged(old, selectionIndex_);
        }

        if (!ic->Evaluate("Submit").Triggered()) return;

        if (selectionIndex_ == 2) {
            requestedAction_ = RequestAction::Quit;
            if (sceneDefaultVariables_ && sceneDefaultVariables_->GetMainWindow()) {
                sceneDefaultVariables_->GetMainWindow()->DestroyNotify();
            }
            return;
        }

        requestedAction_ = (selectionIndex_ == 0) ? RequestAction::Retry : RequestAction::BackToTitle;
        if (auto *out = ctx->GetComponent<SceneChangeOut>()) {
            out->Play();
        }
    }

private:
    void RefreshTexts() {
        if (logoText_) {
            logoText_->SetText("ゲームクリア");
            for (size_t i = 0; i < 128; ++i) {
                auto *sp = (*logoText_)[i];
                if (!sp) continue;
                auto *mat = sp->GetComponent2D<Material2D>();
                if (!mat) continue;
                auto c = mat->GetColor();
                c.x = 0.5f;
                c.y = 1.0f;
                c.z = 0.5f;
                mat->SetColor(c);
            }
        }
        if (touchedGroundText_) {
            touchedGroundText_->SetTextFormat("Touched Ground: {0}", touchedGroundCount_);
        }
        if (clearTimeText_) {
            clearTimeText_->SetTextFormat("Clear Time: {0:.2f}s", static_cast<double>(clearTimeMilliseconds_) / 1000.0);
        }
        if (retryText_) {
            retryText_->SetText(selectionIndex_ == 0 ? "＞ やりなおす" : "  やりなおす");
        }
        if (backToTitleText_) {
            backToTitleText_->SetText(selectionIndex_ == 1 ? "＞ タイトルにもどる" : "  タイトルにもどる");
        }
        if (quitText_) {
            quitText_->SetText(selectionIndex_ == 2 ? "＞ おわる" : "  おわる");
        }
    }

    void RefreshRankingTexts() {
        std::array<ClearTimeBoard::TimeValue, 5> times{};
        size_t timeCount = 0;
        if (clearTimeBoard_) {
            const auto top = clearTimeBoard_->GetTopTimes(5);
            timeCount = top.size();
            for (size_t i = 0; i < top.size() && i < times.size(); ++i) {
                times[i] = top[i];
            }
        }

        for (size_t i = 0; i < rankingTextLines_.size(); ++i) {
            auto *text = rankingTextLines_[i];
            if (!text) continue;
            if (i < timeCount) {
                text->SetTextFormat("{0}. {1:.2f}s", static_cast<int>(i + 1), static_cast<double>(times[i]) / 1000.0);
            } else {
                text->SetText(" ");
            }
            const Vector4 color = (highlightedRankIndex_ == static_cast<int>(i))
                ? Vector4{1.0f, 1.0f, 0.0f, 1.0f}
                : Vector4{1.0f, 1.0f, 1.0f, 1.0f};
            ApplyTextColor(text, color);
        }
    }

    void cacheTextTransforms() {
        std::array<Text *, 5> texts = {logoText_, clearTimeText_, retryText_, backToTitleText_, quitText_};
        for (size_t i = 0; i < texts.size(); ++i) {
            if (!texts[i]) continue;
            auto *tr = texts[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;
            basePositions_[i] = tr->GetTranslate();
            startPositions_[i] = Vector3{basePositions_[i].x + introFromRightOffset_, basePositions_[i].y, basePositions_[i].z};
        }
    }

    void ApplyTextColor(Text *text, const Vector4 &color) {
        if (!text) return;
        for (size_t i = 0; i < 128; ++i) {
            auto *sprite = (*text)[i];
            if (!sprite) continue;
            auto *mat = sprite->GetComponent2D<Material2D>();
            if (!mat) continue;
            Vector4 c = mat->GetColor();
            c.x = color.x;
            c.y = color.y;
            c.z = color.z;
            c.w = color.w;
            mat->SetColor(c);
        }
    }

    void updateEntranceAnimation() {
        std::array<Text *, 5> texts = {logoText_, clearTimeText_, retryText_, backToTitleText_, quitText_};
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

    void updateLogoBobbing() {
        const float t = introElapsed_ * bobSpeed_;
        if (!logoText_) return;
        auto *tr = logoText_->GetComponent2D<Transform2D>();
        if (!tr) return;
        auto pos = tr->GetTranslate();
        pos.y = basePositions_[0].y + std::sin(t) * bobAmplitude_;
        tr->SetTranslate(pos);
    }

    void onSelectionChanged(int oldIndex, int newIndex) {
        if (oldIndex >= 0 && oldIndex < 3) {
            optionAnimElapsed_[static_cast<size_t>(oldIndex)] = optionAnimDuration_;
        }
        if (newIndex >= 0 && newIndex < 3) {
            optionAnimElapsed_[static_cast<size_t>(newIndex)] = 0.0f;
        }
        previousSelectionIndex_ = newIndex;
    }

    void updateOptionSelectionAnimation(float dt) {
        std::array<Text *, 3> options = {retryText_, backToTitleText_, quitText_};
        for (size_t i = 0; i < options.size(); ++i) {
            if (!options[i]) continue;
            auto *tr = options[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;

            const size_t baseIndex = i + 2;
            float y = basePositions_[baseIndex].y;
            if (selectionIndex_ == static_cast<int>(i)) {
                optionAnimElapsed_[i] = std::min(optionAnimDuration_, optionAnimElapsed_[i] + dt);
                const float t = std::clamp(optionAnimElapsed_[i] / std::max(0.0001f, optionAnimDuration_), 0.0f, 1.0f);
                y = EaseOutCubic(basePositions_[baseIndex].y + optionLiftHeight_, basePositions_[baseIndex].y, t);
            } else {
                optionAnimElapsed_[i] = optionAnimDuration_;
            }

            auto pos = tr->GetTranslate();
            pos.y = y;
            tr->SetTranslate(pos);
        }
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
    ClearScoreBoard *scoreboard_ = nullptr;
    ClearTimeBoard *clearTimeBoard_ = nullptr;

    Text *logoText_ = nullptr;
    Text *touchedGroundText_ = nullptr;
    Text *clearTimeText_ = nullptr;
    Text *retryText_ = nullptr;
    Text *backToTitleText_ = nullptr;
    Text *quitText_ = nullptr;
    std::array<Text *, 5> rankingTextLines_{};
    Sprite *backgroundSprite_ = nullptr;

    bool isActive_ = false;
    const bool isTouchGroundUiEnabled_ = false;
    int selectionIndex_ = 0;
    int previousSelectionIndex_ = -1;
    int touchedGroundCount_ = 0;
    int highlightedRankIndex_ = -1;
    ClearTimeBoard::TimeValue clearTimeMilliseconds_ = 0;
    RequestAction requestedAction_ = RequestAction::None;
    float introElapsed_ = 0.0f;
    float introDelaySec_ = 0.08f;
    float introDurationSec_ = 0.35f;
    float introFromRightOffset_ = 420.0f;
    float bobAmplitude_ = 6.0f;
    float bobSpeed_ = 6.0f;
    float optionLiftHeight_ = 16.0f;
    float optionAnimDuration_ = 0.25f;
    std::array<float, 3> optionAnimElapsed_{};
    std::array<Vector3, 5> basePositions_{};
    std::array<Vector3, 5> startPositions_{};
};

} // namespace KashipanEngine
