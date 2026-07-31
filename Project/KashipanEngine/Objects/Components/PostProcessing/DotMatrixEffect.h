#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief ドットマトリクスポストエフェクト
class DotMatrixEffect final : public IPostProcessComponent {
public:
    struct Params {
        float dotSpacing = 8.0f;
        float dotRadius = 3.5f;
        float threshold = 0.0f;
        float intensity = 1.0f;
        bool monochrome = false;
    };

    DotMatrixEffect() : IPostProcessComponent("DotMatrixEffect", GetComponentTypeID<DotMatrixEffect>()) {
        ADD_MEMBER_VARIABLE(params_.dotSpacing);
        ADD_MEMBER_VARIABLE(params_.dotRadius);
        ADD_MEMBER_VARIABLE(params_.threshold);
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.monochrome);
    }
    ~DotMatrixEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<DotMatrixEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.dotmatrixeffect.dot_spacing"), &params_.dotSpacing, 0.1f, 1.0f, 64.0f, "%.1f");
        ImGui::DragFloat(TranslationLabel("component.dotmatrixeffect.dot_radius"), &params_.dotRadius, 0.1f, 0.0f, 64.0f, "%.1f");
        ImGui::DragFloat(TranslationLabel("component.dotmatrixeffect.threshold"), &params_.threshold, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.dotmatrixeffect.intensity"), &params_.intensity, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::Checkbox(TranslationLabel("component.dotmatrixeffect.monochrome"), &params_.monochrome);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["dotSpacing"] = params_.dotSpacing;
        json["dotRadius"] = params_.dotRadius;
        json["threshold"] = params_.threshold;
        json["intensity"] = params_.intensity;
        json["monochrome"] = params_.monochrome;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.dotSpacing = json.value("dotSpacing", 8.0f);
        params_.dotRadius = json.value("dotRadius", 3.5f);
        params_.threshold = json.value("threshold", 0.0f);
        params_.intensity = json.value("intensity", 1.0f);
        params_.monochrome = json.value("monochrome", false);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.dotSpacing = std::max(1.0f, params_.dotSpacing);
        cbData_.dotRadius = std::max(0.0f, params_.dotRadius);
        cbData_.threshold = std::clamp(params_.threshold, 0.0f, 1.0f);
        cbData_.intensity = std::max(0.0f, params_.intensity);
        cbData_.monochrome = params_.monochrome ? 1u : 0u;
        PassInfo pass;
        pass.pipelineName = "PostEffect.DotMatrix";
        pass.constantBufferRequirements = {
            { "Pixel:DotMatrixCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        float dotSpacing = 0.0f;
        float dotRadius = 0.0f;
        float threshold = 0.0f;
        float intensity = 0.0f;
        unsigned int monochrome = 0;
        unsigned int pad = 0;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(DotMatrixEffect)

} // namespace KashipanEngine
