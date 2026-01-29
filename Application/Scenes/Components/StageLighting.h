#pragma once
#include <KashipanEngine.h>

namespace KashipanEngine {

class StageLighting final : public ISceneComponent {
public:
    StageLighting();
    ~StageLighting() override = default;

    void Initialize() override;
    void Finalize() override;
    void Update() override;

    void StartDeadLighting();
    void ResetLighting();

    bool IsDeadLightingActive() const { return isDeadLightingActive_; }

private:
    void UpdateCenterRotateSpotLights(float deltaTime);
    void UpdateRhythmicalSpotLights(float deltaTime);
    void UpdateStageOutsideSpotLights(float deltaTime);

    // dead lighting update that runs while the death sequence is active
    void UpdateDeadLighting(float deltaTime);
    void RandomizeAndEnableSpotLight(SpotLight *spot, bool enabled = true);

    LightManager *lightManager_ = nullptr;

    DirectionalLight *directionalLight_ = nullptr;
    std::vector<SpotLight *> centerRotateSpotLight_;
    std::vector<SpotLight *> rhythmicalSpotLight_;
    std::vector<SpotLight *> stageOutsideSpotLight_;

    Vector4 directionalLightOriginalColor_;
    float directionalLightOriginalIntensity_ = 1.0f;
    Vector4 directionalLightDeadStartColor_{ 1.0f, 0.5f, 0.5f, 1.0f };
    Vector4 directionalLightDeadEndColor_{ 0.0f, 0.0f, 0.0f, 1.0f };
    float directionalLightDeadIntensity_ = 2.0f;

    // dead lighting state
    bool isDeadLightingActive_ = false;
    float deadLightingElapsed_ = 0.0f;
    float deadLightingDuration_ = 2.0f;
};

} // namespace KashipanEngine
