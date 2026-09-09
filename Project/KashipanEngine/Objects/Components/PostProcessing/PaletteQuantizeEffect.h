#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 固定パレット量子化(ファミコン風減色)ポストエフェクト
class PaletteQuantizeEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;
        float ditherAmount = 0.15f;
    };

    PaletteQuantizeEffect() : IPostProcessComponent("PaletteQuantizeEffect", GetComponentTypeID<PaletteQuantizeEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.ditherAmount);
    }
    ~PaletteQuantizeEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<PaletteQuantizeEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.palettequantizeeffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.palettequantizeeffect.dither_amount"), &params_.ditherAmount, 0.01f, 0.0f, 1.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["ditherAmount"] = params_.ditherAmount;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 1.0f);
        params_.ditherAmount = json.value("ditherAmount", 0.15f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.ditherAmount = std::clamp(params_.ditherAmount, 0.0f, 1.0f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.PaletteQuantize";
        pass.constantBufferRequirements = {
            { "Pixel:PaletteQuantizeCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        float intensity = 0.0f;
        float ditherAmount = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(PaletteQuantizeEffect)

} // namespace KashipanEngine
