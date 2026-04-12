#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"

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
    }

private:
    Object3DBase *player_ = nullptr;
    StageDecoPlaneGenerator *decoPlaneGenerator_ = nullptr;
    StageDecoBoxGenerator *decoBoxGenerator_ = nullptr;
};

} // namespace KashipanEngine
