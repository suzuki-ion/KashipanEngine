#pragma once

#include <KashipanEngine.h>
#include "ObjectBatchKeys.h"
#include "Objects/Components/AlwaysRotate.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>

namespace KashipanEngine {

class StageGoalPlaneController final : public ISceneComponent {
public:
    StageGoalPlaneController() : ISceneComponent("StageGoalPlaneController", 1) {}
    ~StageGoalPlaneController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        defaultVars_ = ctx->GetComponent<SceneDefaultVariables>();
        if (!defaultVars_) return;

        auto goal = std::make_unique<Plane3D>();
        goal->SetName("GoalPlane");
        goal->SetBatchKey(ObjectBatchKeys::GoalPlane, RenderType::Instancing);

        if (defaultVars_->GetScreenBuffer3D()) {
            goal->AttachToRenderer(defaultVars_->GetScreenBuffer3D(), "Object3D.Solid.BlendNormal");
        }

        if (auto *mat = goal->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetSampler(SamplerManager::GetSampler(DefaultSampler::LinearWrap));
            mat->SetTexture(TextureManager::GetTextureFromFileName("goal.png"));
            mat->SetColor(Vector4{1.0f, 1.0f, 1.0f, 1.0f});
        }

        goal->RegisterComponent<AlwaysRotate>(Vector3{0.0f, 0.0f, rotateSpeed_});

        if (auto *tr = goal->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3{0.0f, 0.0f, goalZ_});
            tr->SetRotate(Vector3{0.0f, 3.14159265358979323846f, 0.0f});
            tr->SetScale(Vector3{goalScale_, goalScale_, 1.0f});
        }

        goalPlane_ = goal.get();
        (void)ctx->AddObject3D(std::move(goal));

        // ステージの読み込み
        nlohmann::json j;
        std::string stageDataFilePath = ctx->GetSceneVariableOr<std::string>("TargetStageFilePath", "Assets/Application/StageData/stage.json");
        std::ifstream ifs(stageDataFilePath);
        if (ifs.is_open()) {
            try {
                ifs >> j;
            }
            catch (const nlohmann::json::parse_error& e) {
                e;
                assert(false && "JSONファイルのパースに失敗しました。フォーマットを確認してください。");
            }
        } else {
            assert(false && "Failed to open stage data file.");
        }

		// ステージデータからゴールの位置を調整
        if (j.contains("GoalRate")) {
             goalZ_ = goalZ_ * j["GoalRate"].get<float>();
             if (auto* tr = goalPlane_->GetComponent3D<Transform3D>()) {
                 tr->SetTranslate(Vector3{ 0.0f, 0.0f, goalZ_ });
             }
        }

    }

    void Update() override {
        if (!goalPlane_) return;

        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        auto *goalTr = goalPlane_->GetComponent3D<Transform3D>();
        auto *goalMat = goalPlane_->GetComponent3D<Material3D>();
        if (!playerTr || !goalTr || !goalMat) return;

        const float dist = (goalTr->GetTranslate() - playerTr->GetTranslate()).Length();
        const float fadeRange = std::max(0.0001f, farFadeDistance_ - nearFadeDistance_);
        const float alpha = 1.0f - std::clamp((dist - nearFadeDistance_) / fadeRange, 0.0f, 1.0f);
        goalMat->SetColor(Vector4{ 1.0f, 1.0f, 1.0f, alpha * alpha_ });
    }

    float GetGoalZ() const { return goalZ_; }
	float GetConstGoalZ() const { return -8192.0f; }

private:
    SceneDefaultVariables *defaultVars_ = nullptr;
    Object3DBase *goalPlane_ = nullptr;
    Object3DBase *player_ = nullptr;

    float goalZ_ = -8192.0f;
    float goalScale_ = 64.0f * 8.0f * 1.5f;
    float rotateSpeed_ = 0.5f;
    float nearFadeDistance_ = 128.0f;
    float farFadeDistance_ = 2048.0f;
    float alpha_ = 0.3f;
};

} // namespace KashipanEngine
