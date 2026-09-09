#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 走査線(スキャンライン)ポストエフェクト
class ScanlineEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 0.5f;
        float lineSpacing = 2.0f;
        float thickness = 0.5f;
    };

    ScanlineEffect() : IPostProcessComponent("ScanlineEffect", GetComponentTypeID<ScanlineEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.lineSpacing);
        ADD_MEMBER_VARIABLE(params_.thickness);
    }
    ~ScanlineEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScanlineEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.scanlineeffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.scanlineeffect.line_spacing"), &params_.lineSpacing, 0.1f, 1.0f, 32.0f, "%.1f");
        ImGui::DragFloat(TranslationLabel("component.scanlineeffect.thickness"), &params_.thickness, 0.01f, 0.0f, 1.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["lineSpacing"] = params_.lineSpacing;
        json["thickness"] = params_.thickness;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 0.5f);
        params_.lineSpacing = json.value("lineSpacing", 2.0f);
        params_.thickness = json.value("thickness", 0.5f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.lineSpacing = std::max(1.0f, params_.lineSpacing);
        cbData_.thickness = std::clamp(params_.thickness, 0.0f, 1.0f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.Scanline";
        pass.constantBufferRequirements = {
            { "Pixel:ScanlineCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        float intensity = 0.0f;
        float lineSpacing = 0.0f;
        float thickness = 0.0f;
        float pad[3]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(ScanlineEffect)

} // namespace KashipanEngine
