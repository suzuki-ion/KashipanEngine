#include "Objects/SystemObjects/PointLight.h"
#include "Objects/SystemObjects/Camera3D.h"
#include <cstring>

namespace KashipanEngine {

PointLight::PointLight()
    : Object3DBase("PointLight") {
    SetRenderType(RenderType::Standard);
    UpdateLightBufferCPU();

    SetConstantBufferRequirements({ { "Pixel:gPointLight", sizeof(LightBuffer) } });
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            UpdateLightBufferCPU();
            auto **maps = static_cast<void **>(constantBufferMaps);
            std::memcpy(maps[0], &lightBufferCPU_, sizeof(LightBuffer));
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
