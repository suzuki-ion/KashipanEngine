#pragma once
#include <algorithm>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief ディザリングポストエフェクト
class DitherEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;
        bool color = true;
    };

    DitherEffect() : IPostProcessComponent("DitherEffect", GetComponentTypeID<DitherEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.color);
    }
    ~DitherEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<DitherEffect>();
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
        ImGui::DragFloat(TranslationLabel("component.dithereffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::Checkbox(TranslationLabel("component.dithereffect.color"), &params_.color);
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["color"] = params_.color;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 1.0f);
        params_.color = json.value("color", true);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.color = params_.color ? 1u : 0u;
        PassInfo pass;
        pass.pipelineName = "PostEffect.Dither";
        pass.constantBufferRequirements = {
            { "Pixel:DitherCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        float intensity = 0.0f;
        unsigned int color = 0;
        unsigned int pad = 0;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(DitherEffect)

} // namespace KashipanEngine
