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
            p.intensity = Lerp(minIntensity_, maxIntensity_, t);
            radialBlur_->SetParams(p);
        }

        const bool gravitySwitching = (inputHandler && inputHandler->IsGravitySwitching());

        const float targetVignette = gravitySwitching ? gravitySwitchVignetteIntensity_ : 0.0f;
        const float dt = std::max(0.0f, GetDeltaTime());
        vignetteIntensity_ += (targetVignette - vignetteIntensity_) * std::clamp(vignetteLerpSpeed_ * dt * 60.0f, 0.0f, 1.0f);

        if (vignette_) {
            auto v = vignette_->GetParams();
            v.intensity = std::clamp(vignetteIntensity_, 0.0f, 1.0f);
            vignette_->SetParams(v);
        }

        // Apply gravity-switch radial blur intensity override
        if (radialBlur_) {
            auto p = radialBlur_->GetParams();
            const float targetRadial = gravitySwitching ? gravitySwitchRadialBlurIntensity_ : Lerp(minIntensity_, maxIntensity_, t);
            p.intensity = std::clamp(targetRadial, 0.0f, 1.0f);
            radialBlur_->SetParams(p);
        }
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::Begin("PostEffectToPlayerSync");

        ImGui::Text("Vignette (gravity switch)");
        ImGui::SliderFloat("Vignette Intensity", &gravitySwitchVignetteIntensity_, 0.0f, 1.0f);
        ImGui::SliderFloat("Vignette Lerp Speed", &gravitySwitchVignetteLerpSpeed_, 0.0f, 1.0f);

        ImGui::Separator();

        ImGui::Text("Radial Blur (gravity switch)");
        ImGui::SliderFloat("RadialBlur Intensity", &gravitySwitchRadialBlurIntensity_, 0.0f, 1.0f);
        ImGui::SliderFloat("RadialBlur Lerp Speed", &gravitySwitchRadialBlurLerpSpeed_, 0.0f, 1.0f);

        ImGui::End();
    }
#endif

private:
    Object3DBase *player_ = nullptr;
    RadialBlurEffect *radialBlur_ = nullptr;
    VignetteEffect *vignette_ = nullptr;

    float minIntensity_ = 0.5f;
    float maxIntensity_ = 1.0f;
    float vignetteIntensity_ = 0.0f;
    float vignetteLerpSpeed_ = 0.15f;

    float gravitySwitchVignetteIntensity_ = 0.5f;
    float gravitySwitchVignetteLerpSpeed_ = 0.15f;
    float gravitySwitchRadialBlurIntensity_ = 0.75f;
    float gravitySwitchRadialBlurLerpSpeed_ = 0.15f;
};

} // namespace KashipanEngine
