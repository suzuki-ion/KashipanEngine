#include "Objects/SystemObjects/LightManager.h"

#include <algorithm>
#include <cstring>

namespace KashipanEngine {

LightManager::LightManager()
    : Object3DBase("LightManager") {
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

void LightManager::SetPointLights(const std::vector<PointLight*> &lights) {
    pointLights_ = lights;
}

void LightManager::SetSpotLights(const std::vector<SpotLight*> &lights) {
    spotLights_ = lights;
}

void LightManager::AddPointLight(PointLight *light) {
    pointLights_.push_back(light);
}

void LightManager::AddSpotLight(SpotLight *light) {
    spotLights_.push_back(light);
}

void LightManager::RemovePointLight(PointLight *light) {
    pointLights_.erase(std::remove(pointLights_.begin(), pointLights_.end(), light), pointLights_.end());
}

void LightManager::RemoveSpotLight(SpotLight *light) {
    spotLights_.erase(std::remove(spotLights_.begin(), spotLights_.end(), light), spotLights_.end());
}

void LightManager::ClearPointLights() {
    pointLights_.clear();
}

void LightManager::ClearSpotLights() {
    spotLights_.clear();
}

bool LightManager::Render(ShaderVariableBinder &shaderBinder) {
    countsCPU_.pointLightCount = static_cast<std::uint32_t>(pointLights_.size());
    countsCPU_.spotLightCount = static_cast<std::uint32_t>(spotLights_.size());
    (void)shaderBinder;
    return true;
}

} // namespace KashipanEngine
