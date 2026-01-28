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
    const std::vector<SpotLight*>& GetSpotLights() const { return spotLights_; }
    const std::vector<PointLight*>& GetPointLights() const { return pointLights_; }

private:
    LightManager* lightManager_ = nullptr;
    std::vector<SpotLight*> spotLights_;
    std::vector<PointLight*> pointLights_;
};

} // namespace KashipanEngine
