#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/Camera3D.h"
#include <cstring>

namespace KashipanEngine {

PointLight::PointLight()
    : Object3DBase("PointLight") {
    SetRenderType(RenderType::Instancing);
    UpdateLightBufferCPU();

    SetInstanceBufferRequirements({ { "Pixel:gPointLights", sizeof(LightBuffer) } });
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

void PointLight::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

void PointLight::SetColor(const Vector4 &color) {
    color_ = color;
}

void PointLight::SetPosition(const Vector3 &position) {
    position_ = position;
}

void PointLight::SetRange(float range) {
    range_ = range;
}

void PointLight::SetIntensity(float intensity) {
    intensity_ = intensity;
}

void PointLight::SetDecay(float decay) {
    decay_ = decay;
}

void PointLight::UpdateLightBufferCPU() const {
    lightBufferCPU_.enabled = enabled_ ? 1u : 0u;
    lightBufferCPU_.color = color_;
    lightBufferCPU_.position = position_;
    lightBufferCPU_.range = range_;
    lightBufferCPU_.intensity = intensity_;
    lightBufferCPU_.decay = decay_;
}

bool PointLight::Render(ShaderVariableBinder &shaderBinder) {
    (void)shaderBinder;
    UpdateLightBufferCPU();
    return true;
}

} // namespace KashipanEngine
