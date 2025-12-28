#include "Objects/SystemObjects/DirectionalLight.h"
#include <cstring>

namespace KashipanEngine {

DirectionalLight::DirectionalLight()
    : Object3DBase("DirectionalLight") {
    SetRenderType(RenderType::Standard);
    UpdateLightBufferCPU();
}

void DirectionalLight::SetEnabled(bool enabled) {
    enabled_ = enabled;
}

void DirectionalLight::SetColor(const Vector4 &color) {
    color_ = color;
}

void DirectionalLight::SetDirection(const Vector3 &direction) {
    direction_ = direction;
}

void DirectionalLight::SetIntensity(float intensity) {
    intensity_ = intensity;
}

void DirectionalLight::UpdateLightBufferCPU() const {
    lightBufferCPU_.enabled = enabled_ ? 1u : 0u;
    lightBufferCPU_.color = color_;
    lightBufferCPU_.direction = direction_;
    lightBufferCPU_.intensity = intensity_;
}

bool DirectionalLight::Render(ShaderVariableBinder &shaderBinder) {
    (void)shaderBinder;
    // DirectionalLight constant buffer update/bind is handled in Renderer at batch time.
    UpdateLightBufferCPU();
    return true;
}

} // namespace KashipanEngine
