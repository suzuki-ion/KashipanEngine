#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"
#include "Scenes/Components/StageGroundGenerator.h"

#include <algorithm>

namespace KashipanEngine {

class StageObjectController final : public ISceneComponent {
public:
    StageObjectController() : ISceneComponent("StageObjectController", 1) {}
    ~StageObjectController() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        decoPlaneGenerator_ = ctx->GetComponent<StageDecoPlaneGenerator>();
        decoBoxGenerator_ = ctx->GetComponent<StageDecoBoxGenerator>();
        stageGroundGenerator_ = ctx->GetComponent<StageGroundGenerator>();
    }

    void Update() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        if (!player_) {
            player_ = ctx->GetObject3D("PlayerRoot");
        }
        if (!player_) return;

        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (!playerTr) return;

        const Vector3 playerPos = playerTr->GetTranslate();

        if (decoPlaneGenerator_) {
            const float nearFade = decoPlaneGenerator_->GetNearFadeDistance();
            const float farFade = decoPlaneGenerator_->GetFarFadeDistance();
            const float fadeRange = std::max(0.0001f, farFade - nearFade);

            for (const auto &runtime : decoPlaneGenerator_->GetRuntimes()) {
                auto applyFade = [&](Object3DBase *obj) {
                    if (!obj) return;
                    auto *tr = obj->GetComponent3D<Transform3D>();
                    if (!tr) return;

                    const float dist = (tr->GetTranslate() - playerPos).Length();
                    const float alphaT = std::clamp((dist - nearFade) / fadeRange, 0.0f, 1.0f);
                    const float alpha = 1.0f - alphaT;
                    if (auto *mat = obj->GetComponent3D<Material3D>()) {
                        mat->SetColor(Vector4{0.5f, 1.0f, 0.5f, alpha});
                    }
                };

                applyFade(runtime.frontObject);
                applyFade(runtime.backObject);
            }
        }

        if (decoBoxGenerator_) {
            const float nearFade = decoBoxGenerator_->GetNearFadeDistance();
            const float farFade = decoBoxGenerator_->GetFarFadeDistance();
            const float fadeRange = std::max(0.0001f, farFade - nearFade);

            for (const auto &runtime : decoBoxGenerator_->GetRuntimes()) {
                if (!runtime.object) continue;
                auto *tr = runtime.object->GetComponent3D<Transform3D>();
                if (!tr) continue;

                const float dist = (tr->GetTranslate() - playerPos).Length();
                const float alphaT = std::clamp((dist - nearFade) / fadeRange, 0.0f, 1.0f);
                const float alpha = 1.0f - alphaT;
                if (auto *mat = runtime.object->GetComponent3D<Material3D>()) {
                    mat->SetColor(Vector4{0.5f, 1.0f, 0.5f, alpha});
                }
            }
        }

        if (stageGroundGenerator_) {
            for (const auto &groundInfo : stageGroundGenerator_->GetGrounds()) {
                if (!groundInfo.object || !groundInfo.isActive) continue;
                auto *tr = groundInfo.object->GetComponent3D<Transform3D>();
                if (!tr) continue;
                auto *groundDefined = groundInfo.object->GetComponent3D<GroundDefined>();
                if (!groundDefined) continue;
                bool isTouched = groundDefined->HasBeenTouchedByPlayer();
                if (isTouched) continue; // プレイヤーが触れた地面はフェード処理しない

                const float nearFade = groundDefined ? groundDefined->GetNearDistance() : 0.0f;
                const float farFade = groundDefined ? groundDefined->GetFarDistance() : 300.0f;
                const float fadeRange = std::max(0.0001f, farFade - nearFade);
                const Vector4 nearColorScale = groundDefined ? groundDefined->GetNearColorScale() : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };
                const Vector4 farColorScale = groundDefined ? groundDefined->GetFarColorScale() : Vector4{ 0.5f, 0.5f, 0.5f, 1.0f };
                const Vector4 baseColor = groundDefined ? groundDefined->GetDefaultColor() : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f };

                const float dist = (tr->GetTranslate() - playerPos).Length();
                
                // distがfar以下ならスポーンアニメーション
                if (dist <= farFade) {
                    if (auto *spawnAnim = groundInfo.object->GetComponent3D<StageObjectSpawnAnimation>()) {
                        spawnAnim->StartAnimation();
                    }
                }

                const float colorT = std::clamp((dist - nearFade) / fadeRange, 0.0f, 1.0f);

                // 線形補間でカラースケールを計算
                const Vector4 colorScale = Vector4{
                    nearColorScale.x + (farColorScale.x - nearColorScale.x) * colorT,
                    nearColorScale.y + (farColorScale.y - nearColorScale.y) * colorT,
                    nearColorScale.z + (farColorScale.z - nearColorScale.z) * colorT,
                    nearColorScale.w + (farColorScale.w - nearColorScale.w) * colorT
                };

                if (auto *mat = groundInfo.object->GetComponent3D<Material3D>()) {
                    const Vector4 finalColor = Vector4{
                        baseColor.x * colorScale.x,
                        baseColor.y * colorScale.y,
                        baseColor.z * colorScale.z,
                        baseColor.w * colorScale.w
                    };
                    mat->SetColor(finalColor);
                }
            }
        }
    }

private:
    Object3DBase *player_ = nullptr;
    StageDecoPlaneGenerator *decoPlaneGenerator_ = nullptr;
    StageDecoBoxGenerator *decoBoxGenerator_ = nullptr;
    StageGroundGenerator *stageGroundGenerator_ = nullptr;
};

} // namespace KashipanEngine
