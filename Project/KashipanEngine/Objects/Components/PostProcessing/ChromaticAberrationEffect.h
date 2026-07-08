#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief 色収差ポストエフェクト
class ChromaticAberrationEffect final : public IPostProcessComponent {
public:
    struct Params {
        float directionX = 1.0f;
        float directionY = 0.0f;
        float strength = 0.0025f;
    };

    ChromaticAberrationEffect() : IPostProcessComponent("ChromaticAberrationEffect") {}
    ~ChromaticAberrationEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ChromaticAberrationEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Strength", &params_.strength, 0.0001f, 0.0f, 0.05f, "%.5f");
        ImGui::DragFloat2("Direction", &params_.directionX, 0.01f, -1.0f, 1.0f);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["direction"] = { params_.directionX, params_.directionY };
        json["strength"] = params_.strength;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        if (json.contains("direction") && json["direction"].is_array() && json["direction"].size() >= 2) {
            params_.directionX = json["direction"][0].get<float>();
            params_.directionY = json["direction"][1].get<float>();
        }
        params_.strength = json.value("strength", 0.0025f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        cbData_.direction[0] = params_.directionX;
        cbData_.direction[1] = params_.directionY;
        cbData_.strength = params_.strength;
        PassInfo pass;
        pass.pipelineName = "PostEffect.ChromaticAberration";
        pass.constantBufferRequirements = {
            { "Pixel:ChromaticAberrationCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float direction[2]{};
        float strength = 0.0f;
        float pad = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(ChromaticAberrationEffect)

} // namespace KashipanEngine
