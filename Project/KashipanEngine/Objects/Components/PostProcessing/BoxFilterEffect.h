#pragma once
#include <algorithm>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief ボックスフィルタ（ぼかし）ポストエフェクト
class BoxFilterEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;
        int halfSize[2] = { 2, 2 };
    };

    BoxFilterEffect() : IPostProcessComponent("BoxFilterEffect", GetComponentTypeID<BoxFilterEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        AddMemberVariable("params_.halfSize[0]", &params_.halfSize[0]);
        AddMemberVariable("params_.halfSize[1]", &params_.halfSize[1]);
    }
    ~BoxFilterEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<BoxFilterEffect>();
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
        ImGui::DragFloat(TranslationLabel("component.boxfiltereffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragInt2(TranslationLabel("component.boxfiltereffect.half_size"), params_.halfSize, 1.0f, 1);
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["halfSize"] = { params_.halfSize[0], params_.halfSize[1] };
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 1.0f);
        if (json.contains("halfSize") && json["halfSize"].is_array() && json["halfSize"].size() >= 2) {
            params_.halfSize[0] = json["halfSize"][0].get<int>();
            params_.halfSize[1] = json["halfSize"][1].get<int>();
        }
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        auto *owner = GetOwnerScreenBuffer();
        cbData_.invResolution[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.invResolution[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.halfSize[0] = params_.halfSize[0];
        cbData_.halfSize[1] = params_.halfSize[1];
        PassInfo pass;
        pass.pipelineName = "PostEffect.BoxFilter";
        pass.constantBufferRequirements = {
            { "Pixel:BoxFilterCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2]{};
        float intensity = 0.0f;
        float pad = 0.0f;
        int halfSize[2]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(BoxFilterEffect)

} // namespace KashipanEngine
