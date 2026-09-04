#pragma once
#include <algorithm>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief カラー調整ポストエフェクト
class ColorAdjustEffect final : public IPostProcessComponent {
public:
    struct Params {
        float brightness = 0.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;
        float temperature = 0.0f;
        float colorBalance[3] = { 0.0f, 0.0f, 0.0f };
    };

    ColorAdjustEffect() : IPostProcessComponent("ColorAdjustEffect", GetComponentTypeID<ColorAdjustEffect>()) {
        ADD_MEMBER_VARIABLE(params_.brightness);
        ADD_MEMBER_VARIABLE(params_.contrast);
        ADD_MEMBER_VARIABLE(params_.saturation);
        ADD_MEMBER_VARIABLE(params_.temperature);
        AddMemberVariable("params_.colorBalance[0]", &params_.colorBalance[0]);
        AddMemberVariable("params_.colorBalance[1]", &params_.colorBalance[1]);
        AddMemberVariable("params_.colorBalance[2]", &params_.colorBalance[2]);
    }
    ~ColorAdjustEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<ColorAdjustEffect>();
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
        ImGui::DragFloat(TranslationLabel("component.coloradjusteffect.brightness"), &params_.brightness, 0.01f, -1.0f, 1.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.coloradjusteffect.contrast"), &params_.contrast, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.coloradjusteffect.saturation"), &params_.saturation, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.coloradjusteffect.temperature"), &params_.temperature, 0.01f, -2.0f, 2.0f, "%.3f");
        ImGui::DragFloat3(TranslationLabel("component.coloradjusteffect.colorbalance"), params_.colorBalance, 0.01f, -1.0f, 1.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["brightness"] = params_.brightness;
        json["contrast"] = params_.contrast;
        json["saturation"] = params_.saturation;
        json["temperature"] = params_.temperature;
        json["colorBalance"] = { params_.colorBalance[0], params_.colorBalance[1], params_.colorBalance[2] };
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.brightness = json.value("brightness", 0.0f);
        params_.contrast = json.value("contrast", 1.0f);
        params_.saturation = json.value("saturation", 1.0f);
        params_.temperature = json.value("temperature", 0.0f);
        if (json.contains("colorBalance") && json["colorBalance"].is_array() && json["colorBalance"].size() >= 3) {
            for (int i = 0; i < 3; ++i) params_.colorBalance[i] = json["colorBalance"][i].get<float>();
        }
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        cbData_.brightness = std::clamp(params_.brightness, -1.0f, 1.0f);
        cbData_.contrast = std::max(params_.contrast, 0.0f);
        cbData_.saturation = std::max(params_.saturation, 0.0f);
        cbData_.temperature = std::clamp(params_.temperature, -1.0f, 1.0f);
        cbData_.colorBalance[0] = std::clamp(params_.colorBalance[0], -1.0f, 1.0f);
        cbData_.colorBalance[1] = std::clamp(params_.colorBalance[1], -1.0f, 1.0f);
        cbData_.colorBalance[2] = std::clamp(params_.colorBalance[2], -1.0f, 1.0f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.ColorAdjust";
        pass.constantBufferRequirements = {
            { "Pixel:ColorAdjustCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float brightness = 0.0f;
        float contrast = 0.0f;
        float saturation = 0.0f;
        float temperature = 0.0f;
        float colorBalance[3]{};
        float pad = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(ColorAdjustEffect)

} // namespace KashipanEngine
