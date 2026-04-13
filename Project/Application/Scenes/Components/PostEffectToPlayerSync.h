#pragma once

#include <KashipanEngine.h>
#include "Objects/Components/PlayerInputHandler.h"
#include "Objects/Components/PlayerMovementController.h"

#include <algorithm>

namespace KashipanEngine {

class PostEffectToPlayerSync final : public ISceneComponent {
public:
    PostEffectToPlayerSync(Object3DBase *player, RadialBlurEffect *radialBlur, VignetteEffect *vignette)
        : ISceneComponent("PostEffectToPlayerSync", 1),
          player_(player),
          radialBlur_(radialBlur),
          vignette_(vignette) {}

    ~PostEffectToPlayerSync() override = default;

    void Update() override {
        if (!player_) return;

        auto *pm = player_->GetComponent3D<PlayerMovementController>();
        auto *inputHandler = player_->GetComponent3D<PlayerInputHandler>();
        if (!pm) return;

        const float minSpeed = pm->GetMinForwardSpeed();
        const float maxSpeed = pm->GetMaxForwardSpeed();

        float t = 0.0f;
        if (maxSpeed > minSpeed) {
            t = (pm->GetForwardSpeed() - minSpeed) / (maxSpeed - minSpeed);
        }
        t = std::clamp(t, 0.0f, 1.0f);

        if (radialBlur_) {
            auto p = radialBlur_->GetParams();
            p.intensity = minIntensity_ + (maxIntensity_ - minIntensity_) * t;
            radialBlur_->SetParams(p);
        }

        const float targetVignette = (inputHandler && inputHandler->IsGravitySwitching()) ? 0.5f : 0.0f;
        const float dt = std::max(0.0f, GetDeltaTime() * GetGameSpeed());
        vignetteIntensity_ += (targetVignette - vignetteIntensity_) * std::clamp(vignetteLerpSpeed_ * dt * 60.0f, 0.0f, 1.0f);

        if (vignette_) {
            auto v = vignette_->GetParams();
            v.intensity = std::clamp(vignetteIntensity_, 0.0f, 0.5f);
            vignette_->SetParams(v);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {}
#endif

private:
    Object3DBase *player_ = nullptr;
    RadialBlurEffect *radialBlur_ = nullptr;
    VignetteEffect *vignette_ = nullptr;

    float minIntensity_ = 0.5f;
    float maxIntensity_ = 1.0f;
    float vignetteIntensity_ = 0.0f;
    float vignetteLerpSpeed_ = 0.15f;
};

} // namespace KashipanEngine
