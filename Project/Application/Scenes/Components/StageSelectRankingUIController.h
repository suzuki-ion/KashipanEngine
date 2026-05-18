#pragma once

#include <KashipanEngine.h>

#include "Scenes/Components/StageSelectUIController.h"
#include "Utilities/FileIO/JSON.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace KashipanEngine {

class StageSelectRankingUIController final : public ISceneComponent {
public:
    using TimeValue = std::uint64_t;

    StageSelectRankingUIController()
        : ISceneComponent("StageSelectRankingUIController", 1) {}

    ~StageSelectRankingUIController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        auto *defaults = ctx->GetComponent<SceneDefaultVariables>();
        auto *screenBuffer2D = defaults ? defaults->GetScreenBuffer2D() : nullptr;
        if (!screenBuffer2D) return;

        const float screenW = static_cast<float>(screenBuffer2D->GetWidth());
        const float screenH = static_cast<float>(screenBuffer2D->GetHeight());
        const float cx = screenW * 0.25f;
        const float cy = screenH * 0.5f;

        auto header = std::make_unique<Text>(64);
        header->SetName("StageSelectRankingHeader");
        header->SetFont("Assets/Application/Image/KaqookanV2.fnt");
        header->SetText("TOP 10");
        header->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
        header->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        if (auto *tr = header->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector3{cx, cy + 300.0f, 0.0f});
        }
        headerText_ = header.get();
        (void)ctx->AddObject2D(std::move(header));

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto line = std::make_unique<Text>(64);
            line->SetName("StageSelectRankingLine");
            line->SetFont("Assets/Application/Image/KaqookanV2.fnt");
            line->SetText(" ");
            line->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
            line->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
            if (auto *tr = line->GetComponent2D<Transform2D>()) {
                tr->SetTranslate(Vector3{cx, cy + 220.0f - static_cast<float>(i) * 56.0f, 0.0f});
            }
            rankingLineTexts_[i] = line.get();
            (void)ctx->AddObject2D(std::move(line));
        }

        CacheBasePositions();
        LoadFromJsonFile();
        SetVisible(false);
    }

    void Update() override {
        if (!isUpdating_) return;

        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        UpdateEntranceAnimation(dt);

        if (!stageSelectUIController_) return;

        const std::string &selectedPath = stageSelectUIController_->GetSelectedPath();
        if (selectedPath == lastSelectedStagePath_) return;

        lastSelectedStagePath_ = selectedPath;
        const size_t selectedIndex = stageSelectUIController_->GetSelectedIndex();
        UpdateRankingTexts(selectedPath, selectedIndex);
    }

    void SetEnableUpdating(bool enable) {
        if (isUpdating_ == enable) return;

        isUpdating_ = enable;
        if (isUpdating_) {
            LoadFromJsonFile();
            lastSelectedStagePath_.clear();
            SetVisible(true);
            BeginEntrance();
        } else {
            SetVisible(false);
            entranceActive_ = false;
            entranceElapsed_ = 0.0f;
        }
    }

    bool IsUpdating() const { return isUpdating_; }

    void SetStageSelectUIController(StageSelectUIController *controller) {
        stageSelectUIController_ = controller;
    }

private:
    void LoadFromJsonFile(const std::string &filePath = kDefaultFilePath_) {
        stageTimeHistory_.clear();

        const JSON root = LoadJSON(filePath);
        const JSON *stagesNode = &root;
        if (root.is_object() && root.contains("stages") && root["stages"].is_object()) {
            stagesNode = &root["stages"];
        }

        if (!stagesNode->is_object()) return;

        for (auto it = stagesNode->begin(); it != stagesNode->end(); ++it) {
            if (!it.value().is_array()) continue;

            std::vector<TimeValue> times;
            for (const auto &v : it.value()) {
                if (v.is_number_unsigned()) {
                    times.push_back(v.get<TimeValue>());
                } else if (v.is_number_integer()) {
                    const int raw = v.get<int>();
                    times.push_back(static_cast<TimeValue>(std::max(0, raw)));
                } else if (v.is_number_float()) {
                    const double seconds = std::max(0.0, v.get<double>());
                    times.push_back(static_cast<TimeValue>(seconds * 1000.0));
                }
            }

            std::sort(times.begin(), times.end());
            stageTimeHistory_[it.key()] = std::move(times);
        }
    }

    void UpdateRankingTexts(const std::string &stagePath, size_t selectedIndex) {
        if (headerText_) {
            headerText_->SetTextFormat("Stage {0} TOP 10", static_cast<int>(selectedIndex + 1));
        }

        auto it = stageTimeHistory_.find(stagePath);
        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto *text = rankingLineTexts_[i];
            if (!text) continue;

            if (it != stageTimeHistory_.end() && i < it->second.size()) {
                text->SetTextFormat("{0:>2}. {1}", static_cast<int>(i + 1), FormatTime(it->second[i]));
            } else {
                text->SetTextFormat("{0:>2}. --:--.---", static_cast<int>(i + 1));
            }
        }
    }

    std::string FormatTime(TimeValue ms) const {
        const TimeValue minutes = ms / 60000ULL;
        const TimeValue seconds = (ms / 1000ULL) % 60ULL;
        const TimeValue millis = ms % 1000ULL;

        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%02llu:%02llu.%03llu",
            static_cast<unsigned long long>(minutes),
            static_cast<unsigned long long>(seconds),
            static_cast<unsigned long long>(millis));
        return std::string{buf};
    }

    void CacheBasePositions() {
        if (headerText_) {
            if (auto *tr = headerText_->GetComponent2D<Transform2D>()) {
                headerBasePosition_ = tr->GetTranslate();
            }
        }

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto *text = rankingLineTexts_[i];
            if (!text) continue;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) continue;
            rankingLineBasePositions_[i] = tr->GetTranslate();
        }
    }

    void BeginEntrance() {
        entranceActive_ = true;
        entranceElapsed_ = 0.0f;

        if (headerText_) {
            if (auto *tr = headerText_->GetComponent2D<Transform2D>()) {
                auto pos = tr->GetTranslate();
                pos.x = headerBasePosition_.x + introFromLeftOffset_;
                pos.y = headerBasePosition_.y;
                tr->SetTranslate(pos);
            }
            SetTextAlpha(headerText_, 0.0f);
        }

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto *text = rankingLineTexts_[i];
            if (!text) continue;
            if (auto *tr = text->GetComponent2D<Transform2D>()) {
                auto pos = tr->GetTranslate();
                pos.x = rankingLineBasePositions_[i].x + introFromLeftOffset_;
                pos.y = rankingLineBasePositions_[i].y;
                tr->SetTranslate(pos);
            }
            SetTextAlpha(text, 0.0f);
        }
    }

    void UpdateEntranceAnimation(float dt) {
        if (!entranceActive_) return;

        entranceElapsed_ += dt;
        bool allDone = true;

        if (headerText_) {
            auto *tr = headerText_->GetComponent2D<Transform2D>();
            if (tr) {
                const float local = std::clamp(entranceElapsed_ / introDurationSec_, 0.0f, 1.0f);
                auto pos = tr->GetTranslate();
                pos.x = EaseOutCubic(headerBasePosition_.x + introFromLeftOffset_, headerBasePosition_.x, local);
                tr->SetTranslate(pos);
                SetTextAlpha(headerText_, local);
                if (local < 1.0f) allDone = false;
            }
        }

        for (size_t i = 0; i < rankingLineTexts_.size(); ++i) {
            auto *text = rankingLineTexts_[i];
            if (!text) continue;
            auto *tr = text->GetComponent2D<Transform2D>();
            if (!tr) continue;

            const float local = std::clamp((entranceElapsed_ - introDelaySec_ * static_cast<float>(i + 1)) / introDurationSec_, 0.0f, 1.0f);
            auto pos = tr->GetTranslate();
            pos.x = EaseOutCubic(rankingLineBasePositions_[i].x + introFromLeftOffset_, rankingLineBasePositions_[i].x, local);
            tr->SetTranslate(pos);
            SetTextAlpha(text, local);
            if (local < 1.0f) allDone = false;
        }

        if (allDone) {
            entranceActive_ = false;
        }
    }

    void SetVisible(bool visible) {
        const float alpha = visible ? 1.0f : 0.0f;
        SetTextAlpha(headerText_, alpha);
        for (auto *line : rankingLineTexts_) {
            SetTextAlpha(line, alpha);
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
            c.w = std::clamp(alpha, 0.0f, 1.0f);
            mat->SetColor(c);
        }
    }

private:
    inline static const std::string kDefaultFilePath_ = "Assets/Application/ClearTimeBoard.json";

    StageSelectUIController *stageSelectUIController_ = nullptr;

    Text *headerText_ = nullptr;
    std::array<Text *, 10> rankingLineTexts_{};

    bool isUpdating_ = false;
    bool entranceActive_ = false;
    float entranceElapsed_ = 0.0f;
    float introFromLeftOffset_ = -420.0f;
    float introDurationSec_ = 0.35f;
    float introDelaySec_ = 0.05f;

    Vector3 headerBasePosition_{};
    std::array<Vector3, 10> rankingLineBasePositions_{};

    std::string lastSelectedStagePath_;
    std::unordered_map<std::string, std::vector<TimeValue>> stageTimeHistory_{};
};

} // namespace KashipanEngine
