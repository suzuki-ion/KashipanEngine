#pragma once

#include <vector>
#include <memory>
#include <cstdint>

#include "Objects/Object3DBase.h"
#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/SpotLight.h"

namespace KashipanEngine {

class LightManager final : public Object3DBase {
public:
    struct LightCounts {
        std::uint32_t pointLightCount = 0;
        std::uint32_t spotLightCount = 0;
    };

    LightManager();
    ~LightManager() override = default;

    void SetPointLights(const std::vector<PointLight*> &lights);
    void SetSpotLights(const std::vector<SpotLight*> &lights);

    void AddPointLight(PointLight *light);
    void AddSpotLight(SpotLight *light);

    void RemovePointLight(PointLight *light);
    void RemoveSpotLight(SpotLight *light);

    void ClearPointLights();
    void ClearSpotLights();

    const std::vector<PointLight *> &GetPointLights() const { return pointLights_; }
    const std::vector<SpotLight *> &GetSpotLights() const { return spotLights_; }
    const LightCounts &GetLightCountsCPU() const { return countsCPU_; }

protected:
    bool Render(ShaderVariableBinder &shaderBinder) override;

private:
    std::vector<PointLight*> pointLights_;
    std::vector<SpotLight*> spotLights_;

    LightCounts countsCPU_{};
};

} // namespace KashipanEngine
