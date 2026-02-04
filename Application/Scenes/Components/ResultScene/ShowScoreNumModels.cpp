#include "ShowScoreNumModels.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include <algorithm>
#include <string>

namespace KashipanEngine {

ShowScoreNumModels::ShowScoreNumModels()
    : ISceneComponent("ShowScoreNumModels", 1) {
    const float baseY = 2.5f;
    const float lineSpacing = 1.0f;
    const float z = 0.0f;
    centerPositions_[0] = Vector3{ 0.0f, baseY + lineSpacing, z };
    centerPositions_[1] = Vector3{ 0.0f, baseY, z };
    centerPositions_[2] = Vector3{ 0.0f, baseY - lineSpacing, z };
    centerPositions_[3] = Vector3{ 0.0f, baseY - lineSpacing * 3.0f, z };
    rankingTextPosition_ = Vector3{ 0.0f, baseY + lineSpacing * 2.5f, z };
}

void ShowScoreNumModels::Initialize() {
    auto *ctx = GetOwnerContext();
    if (!ctx) return;
    auto *sceneDefaultVariables = ctx->GetComponent<SceneDefaultVariables>();
    if (!sceneDefaultVariables) return;
    auto *screenBuffer3D = sceneDefaultVariables->GetScreenBuffer3D();
    if (!screenBuffer3D) return;

    if (!isInitialized_) {
        for (auto &set : digitModels_) {
            for (auto &slot : set) {
                slot.fill(nullptr);
            }
        }
        rankDigitModels_.fill(nullptr);
        isInitialized_ = true;
    }

    UpdateDigitPositions();

    for (size_t setIndex = 0; setIndex < kSetCount; ++setIndex) {
        for (size_t digitIndex = 0; digitIndex < kDigitCount; ++digitIndex) {
            for (size_t value = 0; value < kDigitVariants; ++value) {
                if (digitModels_[setIndex][digitIndex][value]) continue;
                const std::string fileName = std::to_string(value) + ".obj";
                auto modelHandle = ModelManager::GetModelDataFromFileName(fileName);
                auto obj = std::make_unique<Model>(modelHandle);
                obj->SetUniqueBatchKey();
                obj->SetName("Result.ScoreDigit_" + std::to_string(setIndex) + "_" + std::to_string(digitIndex) + "_" + std::to_string(value));
                obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");

                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(digitPositions_[setIndex][digitIndex]);
                    tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
                    tr->SetScale(digitScale_);
                }

                if (auto *mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 0.0f });
                    mat->SetEnableShadowMapProjection(false);
                }

                digitModels_[setIndex][digitIndex][value] = obj.get();
                ctx->AddObject3D(std::move(obj));
            }
        }
    }

    if (!rankingTextModel_) {
        auto modelHandle = ModelManager::GetModelDataFromFileName("ranking.obj");
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("Result.RankingText");
        obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(rankingTextPosition_);
            tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
            tr->SetScale(digitScale_);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        rankingTextModel_ = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    for (size_t i = 0; i < kRankCount; ++i) {
        if (rankDigitModels_[i]) continue;
        const std::string fileName = std::to_string(static_cast<int>(i) + 1) + ".obj";
        auto modelHandle = ModelManager::GetModelDataFromFileName(fileName);
        auto obj = std::make_unique<Model>(modelHandle);
        obj->SetUniqueBatchKey();
        obj->SetName("Result.RankDigit_" + std::to_string(i + 1));
        obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(rankDigitPositions_[i]);
            tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
            tr->SetScale(digitScale_);
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
            mat->SetEnableShadowMapProjection(false);
        }
        rankDigitModels_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }

    UpdateDigitModels();
}

void ShowScoreNumModels::Update() {
    if (!isInitialized_) return;

    if (positionsDirty_) {
        UpdateDigitPositions();
        for (size_t setIndex = 0; setIndex < kSetCount; ++setIndex) {
            for (size_t digitIndex = 0; digitIndex < kDigitCount; ++digitIndex) {
                for (size_t value = 0; value < kDigitVariants; ++value) {
                    auto *model = digitModels_[setIndex][digitIndex][value];
                    if (!model) continue;
                    if (auto *tr = model->GetComponent3D<Transform3D>()) {
                        tr->SetTranslate(digitPositions_[setIndex][digitIndex]);
                    }
                }
            }
        }
        if (rankingTextModel_) {
            if (auto *tr = rankingTextModel_->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(rankingTextPosition_);
            }
        }
        for (size_t i = 0; i < kRankCount; ++i) {
            auto *model = rankDigitModels_[i];
            if (!model) continue;
            if (auto *tr = model->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(rankDigitPositions_[i]);
            }
        }
        positionsDirty_ = false;
    }

    UpdateDigitModels();
}

void ShowScoreNumModels::SetScores(const std::vector<int> &scores) {
    scores_ = scores;
    UpdateDisplayScores();
    if (isInitialized_) {
        UpdateDigitModels();
    }
}

void ShowScoreNumModels::SetVisible(bool visible) {
    isVisible_ = visible;
    if (isInitialized_) {
        UpdateDigitModels();
    }
}

void ShowScoreNumModels::UpdateDigitPositions() {
    const float halfCount = (static_cast<float>(kDigitCount) - 1.0f) * 0.5f;
    for (size_t setIndex = 0; setIndex < kSetCount; ++setIndex) {
        for (size_t i = 0; i < kDigitCount; ++i) {
            const float offset = (static_cast<float>(i) - halfCount) * digitSpacing_;
            const Vector3 &center = centerPositions_[setIndex];
            digitPositions_[setIndex][i] = Vector3{ center.x + offset, center.y, center.z };
        }
    }

    const float rankOffsetX = halfCount * digitSpacing_ + digitSpacing_ * 1.5f;
    for (size_t i = 0; i < kRankCount; ++i) {
        const Vector3 &center = centerPositions_[i];
        rankDigitPositions_[i] = Vector3{ center.x - rankOffsetX, center.y, center.z };
    }
}

void ShowScoreNumModels::UpdateDisplayScores() {
    displayScores_.fill(0);
    std::vector<int> sortedScores = scores_;
    std::sort(sortedScores.begin(), sortedScores.end(), [](int a, int b) { return a > b; });

    for (size_t i = 0; i < 3 && i < sortedScores.size(); ++i) {
        displayScores_[i] = sortedScores[i];
    }

    if (!scores_.empty()) {
        displayScores_[3] = scores_.back();
    }
}

void ShowScoreNumModels::UpdateDigitModels() {
    const int latestScore = scores_.empty() ? -1 : scores_.back();
    for (size_t setIndex = 0; setIndex < kSetCount; ++setIndex) {
        const int scoreValue = displayScores_[setIndex];
        const int clampedScore = std::clamp(scoreValue, 0, 999999);
        std::array<int, kDigitCount> digits{};
        int temp = clampedScore;
        for (int i = static_cast<int>(kDigitCount) - 1; i >= 0; --i) {
            digits[static_cast<size_t>(i)] = temp % 10;
            temp /= 10;
        }

        const bool highlight = (setIndex < kRankCount) && (latestScore >= 0) && (scoreValue == latestScore);
        for (size_t digitIndex = 0; digitIndex < kDigitCount; ++digitIndex) {
            for (size_t value = 0; value < kDigitVariants; ++value) {
                auto *model = digitModels_[setIndex][digitIndex][value];
                if (!model) continue;
                if (auto *mat = model->GetComponent3D<Material3D>()) {
                    Vector4 color = Vector4{ 1.0f, 1.0f, 1.0f, 0.0f };
                    if (static_cast<int>(value) == digits[digitIndex]) {
                        color.w = 1.0f;
                        if (highlight) {
                            color = Vector4{ 1.0f, 1.0f, 0.5f, 1.0f };
                        }
                    }
                    if (!isVisible_) {
                        color.w = 0.0f;
                    }
                    mat->SetColor(color);
                }
            }
        }
    }

    if (rankingTextModel_) {
        if (auto *mat = rankingTextModel_->GetComponent3D<Material3D>()) {
            Vector4 color = mat->GetColor();
            color.w = isVisible_ ? 1.0f : 0.0f;
            mat->SetColor(color);
        }
    }

    for (size_t i = 0; i < rankDigitModels_.size(); ++i) {
        auto *model = rankDigitModels_[i];
        if (!model) continue;
        if (auto *mat = model->GetComponent3D<Material3D>()) {
            Vector4 color = mat->GetColor();
            color.w = isVisible_ ? 1.0f : 0.0f;
            if (isVisible_ && i < kRankCount && latestScore >= 0 && displayScores_[i] == latestScore) {
                color = Vector4{ 1.0f, 1.0f, 0.5f, 1.0f };
            }
            mat->SetColor(color);
        }
    }
}

} // namespace KashipanEngine
