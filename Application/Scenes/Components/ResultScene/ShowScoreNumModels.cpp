#include "ShowScoreNumModels.h"
#include "Objects/GameObjects/3D/Model.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include <algorithm>
#include <string>

namespace KashipanEngine {

ShowScoreNumModels::ShowScoreNumModels()
    : ISceneComponent("ShowScoreNumModels", 1) {
    centerPositions_[0] = Vector3{ 0.0f, 4.5f, 6.0f };
    centerPositions_[1] = Vector3{ 0.0f, 2.5f, 6.0f };
    centerPositions_[2] = Vector3{ 0.0f, 0.5f, 6.0f };
    centerPositions_[3] = Vector3{ 0.0f, -2.0f, 6.0f };
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
        positionsDirty_ = false;
    }

    UpdateDigitModels();
}

void ShowScoreNumModels::SetScores(const std::vector<float> &scores) {
    scores_ = scores;
    UpdateDisplayScores();
    if (isInitialized_) {
        UpdateDigitModels();
    }
}

void ShowScoreNumModels::SetCenterPosition(size_t setIndex, const Vector3 &center) {
    if (setIndex >= kSetCount) return;
    centerPositions_[setIndex] = center;
    positionsDirty_ = true;
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
}

void ShowScoreNumModels::UpdateDisplayScores() {
    displayScores_.fill(0.0f);
    std::vector<float> sortedScores = scores_;
    std::sort(sortedScores.begin(), sortedScores.end(), [](float a, float b) { return a > b; });

    for (size_t i = 0; i < 3 && i < sortedScores.size(); ++i) {
        displayScores_[i] = sortedScores[i];
    }

    if (!scores_.empty()) {
        displayScores_[3] = scores_.back();
    }
}

void ShowScoreNumModels::UpdateDigitModels() {
    for (size_t setIndex = 0; setIndex < kSetCount; ++setIndex) {
        const float scoreValue = displayScores_[setIndex];
        const int clampedScore = static_cast<int>(std::clamp(scoreValue, 0.0f, 999999.0f));
        std::array<int, kDigitCount> digits{};
        int temp = clampedScore;
        for (int i = static_cast<int>(kDigitCount) - 1; i >= 0; --i) {
            digits[static_cast<size_t>(i)] = temp % 10;
            temp /= 10;
        }

        for (size_t digitIndex = 0; digitIndex < kDigitCount; ++digitIndex) {
            for (size_t value = 0; value < kDigitVariants; ++value) {
                auto *model = digitModels_[setIndex][digitIndex][value];
                if (!model) continue;
                if (auto *mat = model->GetComponent3D<Material3D>()) {
                    Vector4 color = Vector4{ 1.0f, 1.0f, 1.0f, 0.0f };
                    if (static_cast<int>(value) == digits[digitIndex]) {
                        color.w = 1.0f;
                    }
                    mat->SetColor(color);
                }
            }
        }
    }
}

} // namespace KashipanEngine
