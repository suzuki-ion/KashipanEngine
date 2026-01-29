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
    
    LightManager* GetLightManager() const { return lightManager_; }
    const std::vector<SpotLight*>& GetSpotLights() const { return centerRotateSpotLight_; }

    // Update functions for each group
    void UpdateCenterRotateSpotLights(float deltaTime);
    void UpdateRhythmicalSpotLights(float deltaTime);
    void UpdateStageOutsideSpotLights(float deltaTime);

private:
    LightManager* lightManager_ = nullptr;

    // Categorized spot light groups
    std::vector<SpotLight*> centerRotateSpotLight_; // expected 3
    std::vector<SpotLight*> rhythmicalSpotLight_;   // expected 16
    std::vector<SpotLight*> stageOutsideSpotLight_; // expected 16

    // retain point lights vector removed as requested
};

} // namespace KashipanEngine
