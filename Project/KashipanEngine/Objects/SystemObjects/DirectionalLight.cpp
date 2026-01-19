#include "Objects/SystemObjects/DirectionalLight.h"
#include <cstring>

namespace KashipanEngine {

DirectionalLight::DirectionalLight()
    : Object3DBase("DirectionalLight") {
    SetRenderType(RenderType::Standard);
    UpdateLightBufferCPU();

    SetConstantBufferRequirements({ { "Pixel:gDirectionalLight", sizeof(LightBuffer) } });
    SetUpdateConstantBuffersFunction(
        [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            UpdateLightBufferCPU();
            auto **maps = static_cast<void **>(constantBufferMaps);
            std::memcpy(maps[0], &lightBufferCPU_, sizeof(LightBuffer));
            return true;
        });
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
    UpdateLightBufferCPU();
    return true;
}

Matrix4x4 DirectionalLight::ComputeShadowLightViewProjection(float orthoHalfSize, float nearClip, float farClip, Vector3 focus) const {
    Vector3 dir = direction_;
    if (dir.Length() <= 0.0001f) {
        dir = Vector3{0.0f, -1.0f, 0.0f};
    }
    dir = dir.Normalize();

    const float distance = orthoHalfSize * 2.0f;
    const Vector3 eye = focus - dir * distance;

    Vector3 up{0.0f, 1.0f, 0.0f};
    if (std::abs(up.Dot(dir)) > 0.99f) {
        up = Vector3{0.0f, 0.0f, 1.0f};
    }

    Matrix4x4 view = Matrix4x4::Identity();
    view.MakeViewMatrix(eye, focus, up);

    Matrix4x4 proj = Matrix4x4::Identity();
    proj.MakeOrthographicMatrix(-orthoHalfSize, orthoHalfSize, orthoHalfSize, -orthoHalfSize, nearClip, farClip);

    return view * proj;
}

} // namespace KashipanEngine
