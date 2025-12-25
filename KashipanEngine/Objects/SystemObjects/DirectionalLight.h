#pragma once
#include <memory>
#include <string>
#include "Objects/Object3DBase.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Math/Vector4.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class DirectionalLight final : public Object3DBase {
public:
    explicit DirectionalLight(const std::string &name = "DirectionalLight");
    ~DirectionalLight() override = default;

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }

    void SetColor(const Vector4 &color);
    const Vector4 &GetColor() const { return color_; }

    void SetDirection(const Vector3 &direction);
    const Vector3 &GetDirection() const { return direction_; }

    void SetIntensity(float intensity);
    float GetIntensity() const { return intensity_; }

protected:
    bool Render(ShaderVariableBinder &shaderBinder) override;

private:
    struct LightBuffer {
        unsigned int enabled = 0u;
        float pad0[3] = { 0.0f, 0.0f, 0.0f };
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 direction{ 0.0f, -1.0f, 0.0f };
        float intensity = 1.0f;
    };

    void UpdateLightBufferCPU() const;
    void Upload() const;

    bool enabled_ = true;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 direction_{ 0.0f, -1.0f, 0.0f };
    float intensity_ = 1.0f;

    mutable LightBuffer lightBufferCPU_;
    std::unique_ptr<ConstantBufferResource> lightBufferGPU_;
};

} // namespace KashipanEngine
