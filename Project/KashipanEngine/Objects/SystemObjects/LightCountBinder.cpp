#include "Objects/SystemObjects/LightCountBinder.h"

#include <algorithm>
#include <cstring>

namespace KashipanEngine {

LightCountBinder::LightCountBinder()
    : Object3DBase("LightCountBinder") {
    SetRenderType(RenderType::Standard);

    SetConstantBufferRequirements({ { "Pixel:LightCounts", sizeof(LightCounts) } });
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            auto **maps = static_cast<void **>(constantBufferMaps);
            std::memcpy(maps[0], &countsCPU_, sizeof(LightCounts));
            return true;
        });

    SetInstanceBufferRequirements({});
    SetSubmitInstanceFunction([this](void * /*instanceMaps*/, ShaderVariableBinder & /*shaderBinder*/, std::uint32_t /*instanceIndex*/) -> bool {
        return false;
        });
}

void LightCountBinder::SetPointLights(const std::vector<PointLight *> &lights) {
    pointLights_ = lights;
}

void LightCountBinder::SetSpotLights(const std::vector<SpotLight *> &lights) {
    spotLights_ = lights;
}

void LightCountBinder::AddPointLight(PointLight *light) {
    pointLights_.push_back(light);
}

void LightCountBinder::AddSpotLight(SpotLight *light) {
    spotLights_.push_back(light);
}

void LightCountBinder::RemovePointLight(PointLight *light) {
    pointLights_.erase(std::remove(pointLights_.begin(), pointLights_.end(), light), pointLights_.end());
}

void LightCountBinder::RemoveSpotLight(SpotLight *light) {
    spotLights_.erase(std::remove(spotLights_.begin(), spotLights_.end(), light), spotLights_.end());
}

void LightCountBinder::ClearPointLights() {
    pointLights_.clear();
}

void LightCountBinder::ClearSpotLights() {
    spotLights_.clear();
}

bool LightCountBinder::Render(ShaderVariableBinder &shaderBinder) {
    countsCPU_.pointLightCount = static_cast<std::uint32_t>(pointLights_.size());
    countsCPU_.spotLightCount = static_cast<std::uint32_t>(spotLights_.size());
    (void)shaderBinder;
    return true;
}

} // namespace KashipanEngine
