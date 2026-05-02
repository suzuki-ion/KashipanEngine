#pragma once

#include <KashipanEngine.h>
#include "Scenes/Components/StageGroundGenerator.h"
#include "Scenes/Components/StageDecoPlaneGenerator.h"
#include "Scenes/Components/StageDecoBoxGenerator.h"

namespace KashipanEngine {

class StageObjectRandomGenerator final : public ISceneComponent {
public:
    StageObjectRandomGenerator() : ISceneComponent("StageObjectRandomGenerator", 1) {}
    ~StageObjectRandomGenerator() override = default;

    void Initialize() override {
        auto *ctx = GetOwnerContext();
        if (!ctx) return;

        groundGenerator_ = ctx->GetComponent<StageGroundGenerator>();
        decoPlaneGenerator_ = ctx->GetComponent<StageDecoPlaneGenerator>();
        decoBoxGenerator_ = ctx->GetComponent<StageDecoBoxGenerator>();

        if (groundGenerator_) {
            groundGenerator_->RequestGenerate();
        }
        if (decoPlaneGenerator_) {
            decoPlaneGenerator_->RequestGenerate();
        }
        if (decoBoxGenerator_) {
            decoBoxGenerator_->RequestGenerate();
        }
    }

    void Update() override {}

private:
    StageGroundGenerator *groundGenerator_ = nullptr;
    StageDecoPlaneGenerator *decoPlaneGenerator_ = nullptr;
    StageDecoBoxGenerator *decoBoxGenerator_ = nullptr;
};

} // namespace KashipanEngine
