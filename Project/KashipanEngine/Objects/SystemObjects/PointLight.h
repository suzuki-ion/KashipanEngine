#pragma once
#include <memory>
#include <string>
#include "Objects/Object3DBase.h"
#include "Math/Vector4.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class PointLight final : public Object3DBase {
public:
    PointLight();
    ~PointLight() override = default;

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }

    void SetColor(const Vector4 &color);
    const Vector4 &GetColor() const { return color_; }

    void SetPosition(const Vector3 &position);
    const Vector3 &GetPosition() const { return position_; }

    void SetRange(float range);
    float GetRange() const { return range_; }

    void SetIntensity(float intensity);
    float GetIntensity() const { return intensity_; }

    void SetDecay(float decay);
    float GetDecay() const { return decay_; }

    struct LightBuffer {
        unsigned int enabled = 0u;
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        float range = 1.0f;
        float intensity = 1.0f;
        float decay = 2.0f;
    };

    const LightBuffer &GetLightBufferCPU() const { return lightBufferCPU_; }
    void UpdateLightBufferCPUForRenderer() const { UpdateLightBufferCPU(); }

#if defined(USE_IMGUI)
    void ShowImGuiDerived() override {
        ImGui::TextUnformatted(Translation("engine.imgui.pointLight.params").c_str());

        bool enabled = enabled_;
        if (ImGui::Checkbox(Translation("engine.imgui.pointLight.enabled").c_str(), &enabled)) {
            SetEnabled(enabled);
        }

        Vector4 c = color_;
        if (ImGui::ColorEdit4(Translation("engine.imgui.pointLight.color").c_str(), &c.x)) {
            SetColor(c);
        }

        Vector3 p = position_;
        ImGui::DragFloat3(Translation("engine.imgui.pointLight.position").c_str(), &p.x, 0.1f);
        SetPosition(p);

        float range = range_;
        ImGui::DragFloat(Translation("engine.imgui.pointLight.range").c_str(), &range, 0.1f, 0.0f, 1000.0f);
        SetRange(range);

        float intensity = intensity_;
        ImGui::DragFloat(Translation("engine.imgui.pointLight.intensity").c_str(), &intensity, 0.1f, 0.0f, 100.0f);
        SetIntensity(intensity);

        float decay = decay_;
        ImGui::DragFloat(Translation("engine.imgui.pointLight.decay").c_str(), &decay, 0.01f, 0.0f, 10.0f);
        SetDecay(decay);
    }
#endif

protected:
    bool Render(ShaderVariableBinder &shaderBinder) override;

private:
    void UpdateLightBufferCPU() const;

    bool enabled_ = true;
    Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    float range_ = 10.0f;
    float intensity_ = 1.0f;
    float decay_ = 2.0f;

    mutable LightBuffer lightBufferCPU_;
};

} // namespace KashipanEngine
