#include "Objects/SystemObjects/SpotLight.h"
#include <cstring>

namespace KashipanEngine {

SpotLight::SpotLight()
    : Object3DBase("SpotLight") {
    SetRenderType(RenderType::Instancing);
    UpdateLightBufferCPU();

    SetInstanceBufferRequirements({ { "Pixel:gSpotLights", sizeof(LightBuffer) } });
    SetSubmitInstanceFunction(
        [this](void *instanceMaps, ShaderVariableBinder & /*shaderBinder*/, std::uint32_t instanceIndex) -> bool {
            if (!instanceMaps) return false;
            UpdateLightBufferCPU();
            auto **buffers = static_cast<LightBuffer **>(instanceMaps);
            LightBuffer *dst = buffers[0];
            dst[instanceIndex] = lightBufferCPU_;
            return true;
        });
}

void SpotLight::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

void SpotLight::SetColor(const Vector4 &color) {
    color_ = color;
}

void SpotLight::SetPosition(const Vector3 &position) {
    position_ = position;
}

void SpotLight::SetDirection(const Vector3 &direction) {
    direction_ = direction;
}

void SpotLight::SetRange(float range) {
    range_ = range;
}

void SpotLight::SetInnerAngle(float angle) {
    innerAngle_ = angle;
}

void SpotLight::SetOuterAngle(float angle) {
    outerAngle_ = angle;
}

void SpotLight::SetIntensity(float intensity) {
    intensity_ = intensity;
}

void SpotLight::SetDecay(float decay) {
    decay_ = decay;
}

void SpotLight::UpdateLightBufferCPU() const {
    lightBufferCPU_.enabled = enabled_ ? 1u : 0u;
    lightBufferCPU_.color = color_;
    lightBufferCPU_.position = position_;
    lightBufferCPU_.range = range_;
    lightBufferCPU_.direction = direction_;
    lightBufferCPU_.innerAngle = innerAngle_;
    lightBufferCPU_.outerAngle = outerAngle_;
    lightBufferCPU_.intensity = intensity_;
    lightBufferCPU_.decay = decay_;
}

bool SpotLight::Render(ShaderVariableBinder &shaderBinder) {
    (void)shaderBinder;
    UpdateLightBufferCPU();
    return true;
}

} // namespace KashipanEngine
