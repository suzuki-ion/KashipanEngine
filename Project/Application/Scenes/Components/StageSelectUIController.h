#pragma once

#include <KashipanEngine.h>
#include "Utilities/FileIO/JSON.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace KashipanEngine {

class StageSelectUIController final : public ISceneComponent {
public:
    enum class RequestAction {
        None,
        StageSelected,
        Canceled
    };

    StageSelectUIController() : ISceneComponent("StageSelectUIController", 1) {}
    ~StageSelectUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaults = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer2D = defaults ? defaults->GetScreenBuffer2D() : nullptr;
        if (!screenBuffer2D) return;

        const float screenW = static_cast<float>(screenBuffer2D->GetWidth());
        const float screenH = static_cast<float>(screenBuffer2D->GetHeight());
        cx_ = screenW * 0.75f;
        cy_ = screenH * 0.5f;

        // Load stage list
        auto jsonData = LoadJSON("Assets/Application/StageData/StageList.json");
        if (jsonData.is_array()) {
            for (const auto& elem : jsonData) {
                if (elem.is_string()) {
                    stagePaths_.push_back(elem.get<std::string>());
                }
            }
        }

        // Create UI elements
        for (size_t i = 0; i < stagePaths_.size(); ++i) {
            auto text = std::make_unique<Text>(64);
            text->SetName("StageText_" + std::to_string(i));
            text->SetFont("Assets/Application/Image/KaqookanV2.fnt");
            text->SetText("Stage " + std::to_string(i + 1));
            text->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
            text->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
            
            optionTexts_.push_back(text.get());
            ctx->AddObject2D(std::move(text));
        }

        selectionIndex_ = 0;
        currentAnimElapsed_ = selectionAnimDuration_;
        startScrollIndex_ = 0.0f;
        isUpdating_ = true;
        SetEnableUpdating(false);
        UpdatePositions();
    }

    void Update() override {
        if (!isUpdating_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx) return;
        auto *ic = ctx->GetInputCommand();
        if (!ic) return;

        if (stagePaths_.empty()) return;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());

        if (entranceActive_) {
            UpdateEntranceAnimation(dt);
        }

        bool changed = false;
        size_t oldIndex = selectionIndex_;
        if (ic->Evaluate("SelectUp").Triggered()) {
            selectionIndex_ = (selectionIndex_ - 1 + stagePaths_.size()) % stagePaths_.size();
            changed = true;
        }
        if (ic->Evaluate("SelectDown").Triggered()) {
            selectionIndex_ = (selectionIndex_ + 1) % stagePaths_.size();
            changed = true;
        }

        if (changed) {
            float diff = static_cast<float>(selectionIndex_) - static_cast<float>(oldIndex);
            float sizeF = static_cast<float>(stagePaths_.size());
            if (diff > sizeF * 0.5f) diff -= sizeF;
            else if (diff < -sizeF * 0.5f) diff += sizeF;

            startScrollIndex_ = static_cast<float>(selectionIndex_) - diff;
            currentAnimElapsed_ = 0.0f;
        }

        if (ic->Evaluate("Submit").Triggered()) {
            ctx->AddSceneVariable("TargetStageFilePath", stagePaths_[selectionIndex_]);
            requestedAction_ = RequestAction::StageSelected;
            return;
        }
        if (ic->Evaluate("Cancel").Triggered()) {
            requestedAction_ = RequestAction::Canceled;
            return;
        }

        UpdatePositions(dt);
    }

    RequestAction ConsumeRequestedAction() {
        const auto action = requestedAction_;
        requestedAction_ = RequestAction::None;
        return action;
    }

    void SetEnableUpdating(bool enable) {
        if (isUpdating_ != enable) {
            isUpdating_ = enable;
            if (enable) {
                BeginEntrance();
            } else {
                for (auto* text : optionTexts_) {
                    if (auto* tr = text->GetComponent2D<Transform2D>()) {
                        tr->SetScale(Vector3{0.0f, 0.0f, 0.0f});
                    }
                    SetTextAlpha(text, 0.0f);
                }
            }
        }
    }
    bool IsUpdating() const { return isUpdating_; }

    size_t GetSelectedIndex() const { return selectionIndex_; }
    const std::string& GetSelectedPath() const { return stagePaths_[selectionIndex_]; }

private:
    void UpdatePositions(float dt = 0.0f) {
        if (!isUpdating_) return;

        currentAnimElapsed_ = std::min(selectionAnimDuration_, currentAnimElapsed_ + dt);
        float t = std::clamp(currentAnimElapsed_ / std::max(0.0001f, selectionAnimDuration_), 0.0f, 1.0f);

        float visualIndex = EaseOutCubic(startScrollIndex_, static_cast<float>(selectionIndex_), t);

        for (size_t i = 0; i < optionTexts_.size(); ++i) {
            auto *tr = optionTexts_[i]->GetComponent2D<Transform2D>();
            if (!tr) continue;

            float yOffset = static_cast<float>(i) - visualIndex;
            float sizeF = static_cast<float>(stagePaths_.size());
            float halfSize = sizeF * 0.5f;

            while (yOffset > halfSize) yOffset -= sizeF;
            while (yOffset < -halfSize) yOffset += sizeF;

            float localEntrance = 1.0f;
            if (entranceActive_) {
                localEntrance = std::clamp((entranceElapsed_ - introDelaySec_ * static_cast<float>(i)) / introDurationSec_, 0.0f, 1.0f);
            }

            float yPos = cy_ - yOffset * optionMargin_;
            float xPos = cx_;

            if (entranceActive_) {
                xPos = EaseOutCubic(cx_ + introFromRightOffset_, cx_, localEntrance);
            }

            if (i == selectionIndex_) {
                float scale = EaseOutCubic(selectedScalePulse_, selectedScale_, t);
                tr->SetTranslate(Vector3{xPos, yPos, 0.0f});
                tr->SetScale(Vector3{scale, scale, 1.0f});
            } else {
                tr->SetTranslate(Vector3{xPos, yPos, 0.0f});
                tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
            }
        }
    }

    void BeginEntrance() {
        entranceActive_ = true;
        entranceElapsed_ = 0.0f;

        for (size_t i = 0; i < optionTexts_.size(); ++i) {
            auto *text = optionTexts_[i];
            if (!text) continue;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) continue;

            float yOffset = static_cast<float>(i) - static_cast<float>(selectionIndex_);
            float sizeF = static_cast<float>(stagePaths_.size());
            float halfSize = sizeF * 0.5f;

            while (yOffset > halfSize) yOffset -= sizeF;
            while (yOffset < -halfSize) yOffset += sizeF;

            float yPos = cy_ - yOffset * optionMargin_;

            tr->SetTranslate(Vector3{cx_ + introFromRightOffset_, yPos, 0.0f});
            if (i == selectionIndex_) {
                tr->SetScale(Vector3{selectedScalePulse_, selectedScalePulse_, 1.0f});
            } else {
                tr->SetScale(Vector3{1.0f, 1.0f, 1.0f});
            }
            SetTextAlpha(text, 0.0f);
        }
    }

    void UpdateEntranceAnimation(float dt) {
        if (entranceActive_) {
            entranceElapsed_ += dt;
            bool allDone = true;
            for (size_t i = 0; i < optionTexts_.size(); ++i) {
                auto *text = optionTexts_[i];
                if (!text) continue;
                const float local = std::clamp((entranceElapsed_ - introDelaySec_ * static_cast<float>(i)) / introDurationSec_, 0.0f, 1.0f);
                SetTextAlpha(text, local);
                if (local < 1.0f) allDone = false;
            }

            if (allDone) {
                entranceActive_ = false;
            }
        }
    }

    void SetTextAlpha(Text *text, float alpha) {
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

    RequestAction requestedAction_ = RequestAction::None;
    std::vector<std::string> stagePaths_;
    std::vector<Text*> optionTexts_;
    size_t selectionIndex_ = 0;
    float cx_ = 0.0f;
    float cy_ = 0.0f;
    bool isUpdating_ = false;

    float currentAnimElapsed_ = 0.2f;
    float startScrollIndex_ = 0.0f;
    const float selectionAnimDuration_ = 0.2f;

    bool entranceActive_ = false;
    float entranceElapsed_ = 0.0f;
    const float introFromRightOffset_ = 2000.0f;
    const float introDurationSec_ = 0.8f;
    const float introDelaySec_ = 0.1f;

    float optionMargin_ = 100.0f;
    float selectedScale_ = 1.5f;
    float selectedScalePulse_ = 2.0f;
};

} // namespace KashipanEngine
