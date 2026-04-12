#pragma once

#include <KashipanEngine.h>

#include "Scenes/Components/ClearScoreBoard.h"

#include <algorithm>
#include <array>

namespace KashipanEngine {

class TitleSceneUIController final : public ISceneComponent {
public:
    enum class RequestAction {
        None,
        StartGame,
        Quit,
    };

    TitleSceneUIController()
        : ISceneComponent("TitleSceneUIController", 1) {}

    ~TitleSceneUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaults = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer2D = defaults ? defaults->GetScreenBuffer2D() : nullptr;
        if (!screenBuffer2D) return;

        scoreboard_ = ctx->GetComponent<ClearScoreBoard>();

        const float screenW = static_cast<float>(screenBuffer2D->GetWidth());
        const float screenH = static_cast<float>(screenBuffer2D->GetHeight());
        const float cx = screenW * 0.5f;
        const float cy = screenH * 0.5f;

        const float logoY = cy + 308.0f;
        const float startY = logoY - 160.0f - 128.0f;
        const float rankingY = startY - 80.0f;
        const float quitY = rankingY - 80.0f;

        auto title = std::make_unique<Text>(128);
        title->SetName("TitleText");
        title->SetFont("Assets/Application/Image/KaqookanV2_Logo.fnt");
        title->SetText("グランナー");
        title->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        title->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = title->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, logoY, 0.0f});
        }
        titleText_ = title.get();
        (void)ctx->AddObject2D(std::move(title));

        auto start = std::make_unique<Text>(64);
        start->SetName("StartText");
        start->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        start->SetText("＞ スタート");
        start->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        start->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = start->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, startY, 0.0f});
        }
        startText_ = start.get();
        startTextBaseY_ = startY;
        (void)ctx->AddObject2D(std::move(start));

        auto ranking = std::make_unique<Text>(64);
        ranking->SetName("RankingText");
        ranking->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        ranking->SetText("  ランキング");
        ranking->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        ranking->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = ranking->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, rankingY, 0.0f});
        }
        rankingText_ = ranking.get();
        rankingTextBaseY_ = rankingY;
        (void)ctx->AddObject2D(std::move(ranking));

        auto quit = std::make_unique<Text>(64);
        quit->SetName("QuitText");
        quit->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        quit->SetText("  おわる");
        quit->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        quit->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = quit->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, quitY, 0.0f});
        }
        quitText_ = quit.get();
        quitTextBaseY_ = quitY;
        (void)ctx->AddObject2D(std::move(quit));

        auto rankingTitle = std::make_unique<Text>(64);
        rankingTitle->SetName("TitleRankingHeader");
        rankingTitle->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        rankingTitle->SetText("TOP 10");
        rankingTitle->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        rankingTitle->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = rankingTitle->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy + 120.0f, 0.0f});
        }
        rankingHeaderText_ = rankingTitle.get();
        (void)ctx->AddObject2D(std::move(rankingTitle));

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto line = std::make_unique<Text>(64);
            line->SetName("TitleRankingLine");
            line->SetFont("Assets/Application/Image/KaqookanV2.fnt");
            line->SetText(" ");
            line->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
            line->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
            if (auto *tr = line->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector3{cx, cy + 40.0f - static_cast<float>(i) * 56.0f, 0.0f});
            }
            rankingLineTexts_[i] = line.get();
            (void)ctx->AddObject2D(std::move(line));
        }

        cacheEntranceBasePositions();
        setRankingVisible(false);
        beginTitleEntrance();
    }

    RequestAction ConsumeRequestedAction() {
        const auto action = requestedAction_;
        requestedAction_ = RequestAction::None;
        return action;
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        auto *ic = ctx->GetInputCommand();
        if (!ic) return;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());

        updateEntranceAnimation(dt);
        updateSelectionAnimation(dt);

        if (showRanking_) {
            if (ic->Evaluate("Submit").Triggered() || ic->Evaluate("Cancel").Triggered()) {
                showRanking_ = false;
                rankingEntranceActive_ = false;
                setRankingVisible(false);
                refreshOptionTexts();
                beginTitleEntrance();
            }
            return;
        }

        int old = selectionIndex_;
        if (ic->Evaluate("SelectUp").Triggered()) {
            selectionIndex_ = (selectionIndex_ + 2) % 3;
        }
        if (ic->Evaluate("SelectDown").Triggered()) {
            selectionIndex_ = (selectionIndex_ + 1) % 3;
        }
        if (old != selectionIndex_) {
            selectionAnimElapsed_ = 0.0f;
        }

        refreshOptionTexts();

        if (!ic->Evaluate("Submit").Triggered()) return;

        if (selectionIndex_ == 0) {
            requestedAction_ = RequestAction::StartGame;
        } else if (selectionIndex_ == 1) {
            showRanking_ = true;
            updateRankingTexts();
            setRankingVisible(true);
            titleEntranceActive_ = false;
            beginRankingEntrance();
        } else {
            requestedAction_ = RequestAction::Quit;
        }
    }

private:
    void refreshOptionTexts() {
        if (startText_) startText_->SetText(selectionIndex_ == 0 ? "＞ スタート" : "  スタート");
        if (rankingText_) rankingText_->SetText(selectionIndex_ == 1 ? "＞ ランキング" : "  ランキング");
        if (quitText_) quitText_->SetText(selectionIndex_ == 2 ? "＞ おわる" : "  おわる");
    }

    void updateSelectionAnimation(float dt) {
        selectionAnimElapsed_ = std::min(selectionAnimDuration_, selectionAnimElapsed_ + dt);
        const float t = std::clamp(selectionAnimElapsed_ / std::max(0.0001f, selectionAnimDuration_), 0.0f, 1.0f);

        auto apply = [&](Text *text, float baseY, int index) {
            if (!text) return;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) return;
            auto pos = tr->GetTranslate();
            if (selectionIndex_ == index) {
                pos.y = EaseOutCubic(baseY + selectionLift_, baseY, t);
            } else {
                pos.y = baseY;
            }
            tr->SetTranslate(pos);
        };

        apply(startText_, startTextBaseY_, 0);
        apply(rankingText_, rankingTextBaseY_, 1);
        apply(quitText_, quitTextBaseY_, 2);
    }

    void cacheEntranceBasePositions() {
        std::array<Text *, 4> titleTexts = {titleText_, startText_, rankingText_, quitText_};
        for (size_t i = 0; i < titleTexts.size(); ++i) {
            if (!titleTexts[i]) continue;
            auto *tr = titleTexts[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;
            titleBasePositions_[i] = tr->GetTranslate();
        }

        rankingBasePositions_[0] = rankingHeaderText_ && rankingHeaderText_->GetComponent2D<Transform2D>()
            ? rankingHeaderText_->GetComponent2D<Transform2D>()->GetTranslate()
            : Vector3{};
        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            if (!rankingLineTexts_[i]) continue;
            auto *tr = rankingLineTexts_[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;
            rankingBasePositions_[i + 1] = tr->GetTranslate();
        }
    }

    void beginTitleEntrance() {
        titleEntranceActive_ = true;
        titleEntranceElapsed_ = 0.0f;

        std::array<Text *, 4> titleTexts = {titleText_, startText_, rankingText_, quitText_};
        for (size_t i = 1; i < titleTexts.size(); ++i) {
            auto *text = titleTexts[i];
            if (!text) continue;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) continue;
            auto pos = tr->GetTranslate();
            pos.x = titleBasePositions_[i].x + introFromRightOffset_;
            pos.y = titleBasePositions_[i].y;
            tr->SetTranslate(pos);
            setTextAlpha(text, 0.0f);
        }

        if (titleTexts[0]) {
            if (auto *tr = titleTexts[0]->GetComponent2D<Transform2D>()) {
                auto pos = tr->GetTranslate();
                pos.x = titleBasePositions_[0].x;
                pos.y = titleBasePositions_[0].y;
                tr->SetTranslate(pos);
            }
            setTextAlpha(titleTexts[0], 1.0f);
        }
    }

    void beginRankingEntrance() {
        rankingEntranceActive_ = true;
        rankingEntranceElapsed_ = 0.0f;

        if (rankingHeaderText_) {
            if (auto *tr = rankingHeaderText_->GetComponent2D<Transform2D>()) {
                auto pos = tr->GetTranslate();
                pos.x = rankingBasePositions_[0].x + introFromRightOffset_;
                pos.y = rankingBasePositions_[0].y;
                tr->SetTranslate(pos);
            }
            setTextAlpha(rankingHeaderText_, 0.0f);
        }
        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto *text = rankingLineTexts_[i];
            if (!text) continue;
            if (auto *tr = text->GetComponent2D<Transform2D>()) {
                auto pos = tr->GetTranslate();
                pos.x = rankingBasePositions_[i + 1].x + introFromRightOffset_;
                pos.y = rankingBasePositions_[i + 1].y;
                tr->SetTranslate(pos);
            }
            setTextAlpha(text, 0.0f);
        }
    }

    void updateEntranceAnimation(float dt) {
        if (titleEntranceActive_) {
            titleEntranceElapsed_ += dt;
            std::array<Text *, 4> titleTexts = {titleText_, startText_, rankingText_, quitText_};
            bool allDone = true;
            for (size_t i = 1; i < titleTexts.size(); ++i) {
                auto *text = titleTexts[i];
                if (!text) continue;
                auto *tr = text->GetComponent2D<Transform2D>();
                if (!tr) continue;
                const float local = std::clamp((titleEntranceElapsed_ - introDelaySec_ * static_cast<float>(i)) / introDurationSec_, 0.0f, 1.0f);
                auto pos = tr->GetTranslate();
                pos.x = EaseOutCubic(titleBasePositions_[i].x + introFromRightOffset_, titleBasePositions_[i].x, local);
                tr->SetTranslate(pos);
                setTextAlpha(text, local);
                if (local < 1.0f) allDone = false;
            }

            if (titleTexts[0]) {
                if (auto *tr = titleTexts[0]->GetComponent2D<Transform2D>()) {
                    auto pos = tr->GetTranslate();
                    pos.x = titleBasePositions_[0].x;
                    pos.y = titleBasePositions_[0].y;
                    tr->SetTranslate(pos);
                }
                setTextAlpha(titleTexts[0], 1.0f);
            }
            if (allDone) titleEntranceActive_ = false;
        }

        if (rankingEntranceActive_) {
            rankingEntranceElapsed_ += dt;
            bool allDone = true;

            if (rankingHeaderText_) {
                auto *tr = rankingHeaderText_->GetComponent2D<Transform2D>();
                if (tr) {
                    const float local = std::clamp(rankingEntranceElapsed_ / rankingIntroDurationSec_, 0.0f, 1.0f);
                    auto pos = tr->GetTranslate();
                    pos.x = EaseOutCubic(rankingBasePositions_[0].x + introFromRightOffset_, rankingBasePositions_[0].x, local);
                    tr->SetTranslate(pos);
                    setTextAlpha(rankingHeaderText_, local);
                    if (local < 1.0f) allDone = false;
                }
            }

            for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
                auto *text = rankingLineTexts_[i];
                if (!text) continue;
                auto *tr = text->GetComponent2D<Transform2D>();
                if (!tr) continue;
                const float local = std::clamp((rankingEntranceElapsed_ - rankingIntroDelaySec_ * static_cast<float>(i + 1)) / rankingIntroDurationSec_, 0.0f, 1.0f);
                auto pos = tr->GetTranslate();
                pos.x = EaseOutCubic(rankingBasePositions_[i + 1].x + introFromRightOffset_, rankingBasePositions_[i + 1].x, local);
                tr->SetTranslate(pos);
                setTextAlpha(text, local);
                if (local < 1.0f) allDone = false;
            }

            if (allDone) rankingEntranceActive_ = false;
        }
    }

    void setRankingVisible(bool visible) {
        if (rankingHeaderText_) {
            setTextAlpha(rankingHeaderText_, visible ? 1.0f : 0.0f);
        }
        for (auto *line : rankingLineTexts_) {
            if (!line) continue;
            setTextAlpha(line, visible ? 1.0f : 0.0f);
        }

        const float optionsAlpha = visible ? 0.0f : 1.0f;
        setTextAlpha(startText_, optionsAlpha);
        setTextAlpha(rankingText_, optionsAlpha);
        setTextAlpha(quitText_, optionsAlpha);
    }

    void updateRankingTexts() {
        std::array<int, 10> top{};
        if (scoreboard_) {
            auto scores = scoreboard_->GetTopScores(10);
            for (size_t i = 0; i < scores.size() && i < top.size(); ++i) {
                top[i] = scores[i];
            }
        }

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            if (!rankingLineTexts_[i]) continue;
            rankingLineTexts_[i]->SetTextFormat("{0:>2}. {1}", static_cast<int>(i + 1), top[i]);
        }
    }

    void setTextAlpha(Text *text, float alpha) {
        if (!text) return;
        for (size_t i = 0; i < 128; ++i) {
            auto *sp = (*text)[i];
            if (!sp) continue;
            auto *mat = sp->GetComponent2D<Material2D>();
            if (!mat) continue;
            auto c = mat->GetColor();
            c.w = alpha;
            mat->SetColor(c);
        }
    }

private:
    ClearScoreBoard *scoreboard_ = nullptr;

    Text *titleText_ = nullptr;
    Text *startText_ = nullptr;
    Text *rankingText_ = nullptr;
    Text *quitText_ = nullptr;
    Text *rankingHeaderText_ = nullptr;
    std::array<Text *, 10> rankingLineTexts_{};

    int selectionIndex_ = 0;
    float selectionAnimElapsed_ = 0.25f;
    float selectionAnimDuration_ = 0.25f;
    float selectionLift_ = 16.0f;
    float startTextBaseY_ = 0.0f;
    float rankingTextBaseY_ = 0.0f;
    float quitTextBaseY_ = 0.0f;

    bool showRanking_ = false;
    RequestAction requestedAction_ = RequestAction::None;

    float introDelaySec_ = 0.08f;
    float introDurationSec_ = 0.35f;
    float introFromRightOffset_ = 420.0f;
    float rankingIntroDelaySec_ = 0.05f;
    float rankingIntroDurationSec_ = 0.2f;
    bool titleEntranceActive_ = false;
    float titleEntranceElapsed_ = 0.0f;
    bool rankingEntranceActive_ = false;
    float rankingEntranceElapsed_ = 0.0f;

    std::array<Vector3, 4> titleBasePositions_{};
    std::array<Vector3, 11> rankingBasePositions_{};
};

} // namespace KashipanEngine
