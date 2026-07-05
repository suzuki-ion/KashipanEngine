#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief グレースケール化ポストエフェクト
class GrayscaleEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;
    };

    GrayscaleEffect() : IPostProcessComponent("GrayscaleEffect") {}
    ~GrayscaleEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<GrayscaleEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["intensity"] = params_.intensity;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        params_.intensity = json.value("intensity", 1.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
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
