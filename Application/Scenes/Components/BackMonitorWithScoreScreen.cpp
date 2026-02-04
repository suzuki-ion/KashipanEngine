#include "BackMonitorWithScoreScreen.h"
#include "BackMonitor.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include <algorithm>
#include <string>

namespace KashipanEngine {

BackMonitorWithScoreScreen::BackMonitorWithScoreScreen(ScreenBuffer *target, ScoreManager *scoreManager)
    : BackMonitorRenderer("BackMonitorWithScoreScreen", target), scoreManager_(scoreManager) {}

BackMonitorWithScoreScreen::~BackMonitorWithScoreScreen() {}

void BackMonitorWithScoreScreen::Initialize() {
    auto target = GetTargetScreenBuffer();
    if (!target) return;
    auto ctx = GetOwnerContext();
    if (!ctx) return;

    if (!isInitialized_) {
        for (auto &slot : digitModels_) {
            slot.fill(nullptr);
        }
        isInitialized_ = true;
    }

    const float halfCount = (static_cast<float>(kDigitCount) - 1.0f) * 0.5f;
    for (size_t i = 0; i < kDigitCount; ++i) {
        const float offset = (static_cast<float>(i) - halfCount) * digitSpacing_;
        digitPositions_[i] = Vector3{ centerPosition_.x + offset, centerPosition_.y, centerPosition_.z };
    }

    for (size_t digitIndex = 0; digitIndex < kDigitCount; ++digitIndex) {
        for (size_t value = 0; value < kDigitVariants; ++value) {
            if (digitModels_[digitIndex][value]) continue;
            const std::string fileName = std::to_string(value) + ".obj";
            auto modelHandle = ModelManager::GetModelDataFromFileName(fileName);
            auto obj = std::make_unique<Model>(modelHandle);
            obj->SetUniqueBatchKey();
            obj->SetName("BackMonitor.ScoreDigit_" + std::to_string(digitIndex) + "_" + std::to_string(value));
            obj->AttachToRenderer(target, "Object3D.Solid.BlendNormal");

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(digitPositions_[digitIndex]);
                tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
                tr->SetScale(digitScale_);
            }

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 0.0f });
                mat->SetEnableShadowMapProjection(false);
            }

            digitModels_[digitIndex][value] = obj.get();
            ctx->AddObject3D(std::move(obj));
        }
    }

    wasActive_ = false;
}

void BackMonitorWithScoreScreen::Update() {
    if (!IsActive()) {
        if (wasActive_) {
            Initialize();
        }
        for (const auto &slot : digitModels_) {
            for (auto *model : slot) {
                if (!model) continue;
                if (auto *mat = model->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4{ 0.0f, 0.0f, 0.0f, 0.0f });
                }
            }
        }
        wasActive_ = false;
        return;
    }

    if (!wasActive_) {
        wasActive_ = true;
        for (size_t i = 0; i < kDigitCount; ++i) {
            for (size_t v = 0; v < kDigitVariants; ++v) {
                auto *model = digitModels_[i][v];
                if (!model) continue;
                if (auto *tr = model->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(digitPositions_[i]);
                }
            }
        }
    }

    if (!scoreManager_) return;
    auto bm = GetBackMonitor();
    if (!bm || !bm->IsReady()) return;

    const float dt = GetDeltaTime();

    const int currentScore = scoreManager_->GetScore();
    if (currentScore != lastScore_) {
        lastScore_ = currentScore;
        scaleAnimElapsed_ = 0.0f;
        scaleAnimating_ = true;
    }

    score_ = static_cast<float>(currentScore);
    displayScore_ = Lerp(displayScore_, score_, 0.1f);
    if (score_ - displayScore_ < 0.5f) {
        displayScore_ = score_;
    }

    float scaleY = digitScale_.y;
    if (scaleAnimating_) {
        scaleAnimElapsed_ += dt;
        const float t = Normalize01(scaleAnimElapsed_, 0.0f, scaleAnimDuration_);
        scaleY = EaseOutCubic(scaleAnimStartY_, digitScale_.y, t);
        if (t >= 1.0f) {
            scaleAnimating_ = false;
            scaleY = digitScale_.y;
        }
    }

    const int clampedScore = static_cast<int>(std::clamp(displayScore_, 0.0f, 999999.0f));
    std::array<int, kDigitCount> digits{};
    int temp = clampedScore;
    for (int i = static_cast<int>(kDigitCount) - 1; i >= 0; --i) {
        digits[static_cast<size_t>(i)] = temp % 10;
        temp /= 10;
    }

    bool hasNonZero = false;
    for (size_t i = 0; i < kDigitCount; ++i) {
        if (digits[i] != 0) hasNonZero = true;
        const bool visible = hasNonZero || i == kDigitCount - 1;
        const bool isUnused = !visible;
        for (size_t v = 0; v < kDigitVariants; ++v) {
            auto *model = digitModels_[i][v];
            if (!model) continue;
            if (auto *mat = model->GetComponent3D<Material3D>()) {
                float alpha = 0.0f;
                Vector4 color = Vector4{ 1.0f, 1.0f, 1.0f, 0.0f };
                if (isUnused && v == 0) {
                    alpha = 1.0f;
                    color = Vector4{ 0.5f, 0.5f, 0.5f, 1.0f };
                } else if (!isUnused && static_cast<int>(v) == digits[i]) {
                    alpha = 1.0f;
                    color = Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
                }
                color.w = alpha;
                mat->SetColor(color);
            }
            if (auto *tr = model->GetComponent3D<Transform3D>()) {
                Vector3 scale = digitScale_;
                if (!isUnused) {
                    scale.y = scaleY;
                }
                tr->SetScale(scale);
            }
        }
    }
}

} // namespace KashipanEngine
