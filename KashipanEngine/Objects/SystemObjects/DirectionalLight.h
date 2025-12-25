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

#if defined(USE_IMGUI)
    void ShowImGuiDerived() override {
        ImGui::TextUnformatted(Translation("engine.imgui.directionalLight.params").c_str());

        bool enabled = enabled_;
        if (ImGui::Checkbox(Translation("engine.imgui.directionalLight.enabled").c_str(), &enabled)) {
            SetEnabled(enabled);
        }

        Vector4 c = color_;
        if (ImGui::ColorEdit4(Translation("engine.imgui.directionalLight.color").c_str(), &c.x)) {
            SetColor(c);
        }

        Vector3 d = direction_;
        ImGui::DragFloat3(Translation("engine.imgui.directionalLight.direction").c_str(), &d.x, 0.05f);
        SetDirection(d);

        float intensity = intensity_;
        ImGui::DragFloat(Translation("engine.imgui.directionalLight.intensity").c_str(), &intensity, 0.1f, 0.0f, 10.0f);
        SetIntensity(intensity);
    }
#endif

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
