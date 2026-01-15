#pragma once
#include <memory>
#include <string>
#include "Objects/Object3DBase.h"
#include "Math/Vector4.h"
#include "Math/Vector3.h"

namespace KashipanEngine {

class SpotLight final : public Object3DBase {
public:
    SpotLight();
    ~SpotLight() override = default;

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }

    void SetColor(const Vector4 &color);
    const Vector4 &GetColor() const { return color_; }

    void SetPosition(const Vector3 &position);
    const Vector3 &GetPosition() const { return position_; }

    void SetDirection(const Vector3 &direction);
    const Vector3 &GetDirection() const { return direction_; }

    void SetRange(float range);
    float GetRange() const { return range_; }

    void SetInnerAngle(float angle);
    float GetInnerAngle() const { return innerAngle_; }

    void SetOuterAngle(float angle);
    float GetOuterAngle() const { return outerAngle_; }

    void SetIntensity(float intensity);
    float GetIntensity() const { return intensity_; }

    void SetDecay(float decay);
    float GetDecay() const { return decay_; }

    struct LightBuffer {
        unsigned int enabled = 0u;
        float pad0[3] = { 0.0f, 0.0f, 0.0f };
        Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Vector3 position{ 0.0f, 0.0f, 0.0f };
        float range = 1.0f;
        Vector3 direction{ 0.0f, -1.0f, 0.0f };
        float innerAngle = 0.0f;
        float outerAngle = 0.0f;
        float intensity = 1.0f;
        float decay = 1.0f;
    };

    const LightBuffer &GetLightBufferCPU() const { return lightBufferCPU_; }
    void UpdateLightBufferCPUForRenderer() const { UpdateLightBufferCPU(); }

#if defined(USE_IMGUI)
    void ShowImGuiDerived() override {
        ImGui::TextUnformatted(Translation("engine.imgui.spotLight.params").c_str());

        bool enabled = enabled_;
        if (ImGui::Checkbox(Translation("engine.imgui.spotLight.enabled").c_str(), &enabled)) {
            SetEnabled(enabled);
        }

        Vector4 c = color_;
        if (ImGui::ColorEdit4(Translation("engine.imgui.spotLight.color").c_str(), &c.x)) {
            SetColor(c);
        }

        Vector3 p = position_;
        ImGui::DragFloat3(Translation("engine.imgui.spotLight.position").c_str(), &p.x, 0.1f);
        SetPosition(p);

        Vector3 d = direction_;
        ImGui::DragFloat3(Translation("engine.imgui.spotLight.direction").c_str(), &d.x, 0.05f);
        SetDirection(d);

        float range = range_;
        ImGui::DragFloat(Translation("engine.imgui.spotLight.range").c_str(), &range, 0.1f, 0.0f, 1000.0f);
        SetRange(range);

        float innerAngle = innerAngle_;
        ImGui::DragFloat(Translation("engine.imgui.spotLight.innerAngle").c_str(), &innerAngle, 0.01f, 0.0f, 3.14f);
        SetInnerAngle(innerAngle);

        float outerAngle = outerAngle_;
        ImGui::DragFloat(Translation("engine.imgui.spotLight.outerAngle").c_str(), &outerAngle, 0.01f, 0.0f, 3.14f);
        SetOuterAngle(outerAngle);

        float intensity = intensity_;
        ImGui::DragFloat(Translation("engine.imgui.spotLight.intensity").c_str(), &intensity, 0.1f, 0.0f, 100.0f);
        SetIntensity(intensity);

        float decay = decay_;
        ImGui::DragFloat(Translation("engine.imgui.spotLight.decay").c_str(), &decay, 0.01f, 0.0f, 10.0f);
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
    Vector3 direction_{ 0.0f, -1.0f, 0.0f };
    float range_ = 10.0f;
    float innerAngle_ = 0.4f;
    float outerAngle_ = 0.6f;
    float intensity_ = 1.0f;
    float decay_ = 1.0f;

    mutable LightBuffer lightBufferCPU_;
};

} // namespace KashipanEngine
