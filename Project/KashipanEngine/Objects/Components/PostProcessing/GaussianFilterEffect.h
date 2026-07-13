#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief ガウシアンフィルタ（ぼかし）ポストエフェクト
class GaussianFilterEffect final : public IPostProcessComponent {
public:
    struct Params {
        int radius = 3;
        float sigma = 1.0f;
    };

    GaussianFilterEffect() : IPostProcessComponent("GaussianFilterEffect") {}
    ~GaussianFilterEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<GaussianFilterEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragInt("Radius", &params_.radius, 1.0f, 0, 32);
        ImGui::DragFloat("Sigma", &params_.sigma, 0.01f, 0.01f, 32.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["radius"] = params_.radius;
        json["sigma"] = params_.sigma;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.radius = json.value("radius", 3);
        params_.sigma = json.value("sigma", 1.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.radius = std::clamp(params_.radius, 0, 32);
        cbData_.sigma = std::max(params_.sigma, 0.0001f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.GaussianFilter";
        pass.constantBufferRequirements = {
            { "Pixel:GaussianCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        int radius = 0;
        float sigma = 0.0f;
        float pad = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(GaussianFilterEffect)

} // namespace KashipanEngine
