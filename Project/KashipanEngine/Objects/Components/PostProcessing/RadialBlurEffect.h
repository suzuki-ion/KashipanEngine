#pragma once
#include <algorithm>
#include <cstdint>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief ラジアルブラーポストエフェクト
class RadialBlurEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 0.25f;
        int sampleCount = 8;
        float radialCenter[2] = { 0.5f, 0.5f };
        float startRadius = 0.0f;
    };

    RadialBlurEffect() : IPostProcessComponent("RadialBlurEffect", GetComponentTypeID<RadialBlurEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.sampleCount);
        AddMemberVariable("params_.radialCenter[0]", &params_.radialCenter[0]);
        AddMemberVariable("params_.radialCenter[1]", &params_.radialCenter[1]);
        ADD_MEMBER_VARIABLE(params_.startRadius);
    }
    ~RadialBlurEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<RadialBlurEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.radialblureffect.intensity"), &params_.intensity, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::DragInt(TranslationLabel("component.radialblureffect.sample_count"), &params_.sampleCount, 1.0f, 1, 64);
        ImGui::DragFloat2(TranslationLabel("component.radialblureffect.center"), params_.radialCenter, 0.001f, -1.0f, 2.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.radialblureffect.start_radius"), &params_.startRadius, 0.001f, 0.0f, 2.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["sampleCount"] = params_.sampleCount;
        json["radialCenter"] = { params_.radialCenter[0], params_.radialCenter[1] };
        json["startRadius"] = params_.startRadius;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 0.25f);
        params_.sampleCount = json.value("sampleCount", 8);
        if (json.contains("radialCenter") && json["radialCenter"].is_array() && json["radialCenter"].size() >= 2) {
            params_.radialCenter[0] = json["radialCenter"][0].get<float>();
            params_.radialCenter[1] = json["radialCenter"][1].get<float>();
        }
        params_.startRadius = json.value("startRadius", 0.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        cbData_.intensity = std::max(0.0f, params_.intensity);
        cbData_.sampleCount = static_cast<std::uint32_t>(std::max(1, params_.sampleCount));
        cbData_.radialCenter[0] = params_.radialCenter[0];
        cbData_.radialCenter[1] = params_.radialCenter[1];
        cbData_.startRadius = std::max(0.0f, params_.startRadius);
        PassInfo pass;
        pass.pipelineName = "PostEffect.RadialBlur";
        pass.constantBufferRequirements = {
            { "Pixel:RadialBlurCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float intensity = 0.0f;
        std::uint32_t sampleCount = 0;
        float radialCenter[2]{};
        float startRadius = 0.0f;
        float pad[3]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(RadialBlurEffect)

} // namespace KashipanEngine
