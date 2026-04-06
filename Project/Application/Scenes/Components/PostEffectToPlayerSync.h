#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerMovementController.h"

namespace KashipanEngine {

class PostEffectToPlayerSync final : public ISceneComponent {
public:
    PostEffectToPlayerSync(Object3DBase *player, RadialBlurEffect *radialBlur)
        : ISceneComponent("PostEffectToPlayerSync", 1),
          player_(player),
          radialBlur_(radialBlur) {}

    ~PostEffectToPlayerSync() override = default;

    void Update() override {
        if (!player_ || !radialBlur_) return;

        auto *pm = player_->GetComponent3D<PlayerMovementController>();
        if (!pm) return;

        const float minSpeed = pm->GetMinForwardSpeed();
        const float maxSpeed = pm->GetMaxForwardSpeed();

        float t = 0.0f;
        if (maxSpeed > minSpeed) {
            t = (pm->GetForwardSpeed() - minSpeed) / (maxSpeed - minSpeed);
        }
        t = std::clamp(t, 0.0f, 1.0f);

        auto p = radialBlur_->GetParams();
        p.intensity = minIntensity_ + (maxIntensity_ - minIntensity_) * t;
        radialBlur_->SetParams(p);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Object3DBase *player_ = nullptr;
    RadialBlurEffect *radialBlur_ = nullptr;

    float minIntensity_ = 0.0f;
    float maxIntensity_ = 0.5f;
};

} // namespace KashipanEngine
