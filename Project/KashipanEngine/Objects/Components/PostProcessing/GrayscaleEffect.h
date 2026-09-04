#pragma once
#include <algorithm>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief グレースケール化ポストエフェクト
class GrayscaleEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;
    };

    GrayscaleEffect() : IPostProcessComponent("GrayscaleEffect", GetComponentTypeID<GrayscaleEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
    }
    ~GrayscaleEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<GrayscaleEffect>();
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
        ImGui::DragFloat(TranslationLabel("component.grayscaleeffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 1.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.Grayscale";
        pass.constantBufferRequirements = {
            { "Pixel:GrayscaleCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float intensity = 1.0f;
        float pad[3]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(GrayscaleEffect)

} // namespace KashipanEngine
