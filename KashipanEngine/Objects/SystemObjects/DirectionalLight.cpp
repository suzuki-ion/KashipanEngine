#include "Objects/SystemObjects/DirectionalLight.h"
#include <cstring>

namespace KashipanEngine {

DirectionalLight::DirectionalLight(const std::string &name)
    : Object3DBase(name) {
    lightBufferGPU_ = std::make_unique<ConstantBufferResource>(sizeof(LightBuffer));
    UpdateLightBufferCPU();
    Upload();
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

void DirectionalLight::Upload() const {
    if (!lightBufferGPU_) return;
    void *mapped = lightBufferGPU_->Map();
    if (!mapped) return;
    std::memcpy(mapped, &lightBufferCPU_, sizeof(LightBuffer));
    lightBufferGPU_->Unmap();
}

bool DirectionalLight::Render(ShaderVariableBinder &shaderBinder) {
    if (!lightBufferGPU_) return false;

    UpdateLightBufferCPU();
    Upload();

    return shaderBinder.Bind("Pixel:gDirectionalLight", lightBufferGPU_.get());
}

} // namespace KashipanEngine
