#pragma once
#include <algorithm>
#include "Math/Vector4.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief ビネット（周辺減光）ポストエフェクト
class VignetteEffect final : public IPostProcessComponent {
public:
    struct Params {
        float center[2] = { 0.5f, 0.5f };
        Vector4 color{ 0.0f, 0.0f, 0.0f, 1.0f };
        float intensity = 0.0f;
        float innerRadius = 0.3f;
        float smoothness = 0.3f;
    };

    VignetteEffect() : IPostProcessComponent("VignetteEffect") {}
    ~VignetteEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<VignetteEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat2("Center", params_.center, 0.001f, -2.0f, 2.0f, "%.3f");
        ImGui::ColorEdit4("Color", &params_.color.x);
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Inner Radius", &params_.innerRadius, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Smoothness", &params_.smoothness, 0.001f, 0.0001f, 2.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["center"] = { params_.center[0], params_.center[1] };
        json["color"] = ToJSON(params_.color);
        json["intensity"] = params_.intensity;
        json["innerRadius"] = params_.innerRadius;
        json["smoothness"] = params_.smoothness;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        if (json.contains("center") && json["center"].is_array() && json["center"].size() >= 2) {
            params_.center[0] = json["center"][0].get<float>();
            params_.center[1] = json["center"][1].get<float>();
        }
        if (json.contains("color")) params_.color = FromJSON<Vector4>(json["color"]);
        params_.intensity = json.value("intensity", 0.0f);
        params_.innerRadius = json.value("innerRadius", 0.3f);
        params_.smoothness = json.value("smoothness", 0.3f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        cbData_.center[0] = params_.center[0];
        cbData_.center[1] = params_.center[1];
        cbData_.color = params_.color;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.innerRadius = std::clamp(params_.innerRadius, 0.0f, 1.0f);
        cbData_.smoothness = std::max(0.0001f, params_.smoothness);
        PassInfo pass;
        pass.pipelineName = "PostEffect.Vignette";
        pass.constantBufferRequirements = {
            { "Pixel:VignetteCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float center[2]{};
        float pad0[2]{};
        Vector4 color;
        float intensity = 0.0f;
        float innerRadius = 0.0f;
        float smoothness = 0.0f;
        float pad1 = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(VignetteEffect)

} // namespace KashipanEngine
