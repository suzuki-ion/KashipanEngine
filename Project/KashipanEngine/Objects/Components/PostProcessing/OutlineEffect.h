#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief 輪郭線ポストエフェクト
class OutlineEffect final : public IPostProcessComponent {
public:
    struct Params {
        float threshold = 0.1f;
        float thickness = 1.0f;
        float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        float cameraNear = 0.1f;
        float cameraFar = 1000.0f;
    };

    OutlineEffect() : IPostProcessComponent("OutlineEffect") {
        ADD_MEMBER_VARIABLE(params_.threshold);
        ADD_MEMBER_VARIABLE(params_.thickness);
        AddMemberVariable("params_.color[0]", &params_.color[0]);
        AddMemberVariable("params_.color[1]", &params_.color[1]);
        AddMemberVariable("params_.color[2]", &params_.color[2]);
        AddMemberVariable("params_.color[3]", &params_.color[3]);
    }
    ~OutlineEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<OutlineEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Threshold", &params_.threshold, 0.001f, 0.0f, 10.0f, "%.4f");
        ImGui::DragFloat("Thickness", &params_.thickness, 0.1f, 0.0f, 16.0f, "%.1f");
        ImGui::ColorEdit4("Color", params_.color);
        ImGui::DragFloat("Camera Near", &params_.cameraNear, 0.01f, 0.0001f, 100.0f, "%.4f");
        ImGui::DragFloat("Camera Far", &params_.cameraFar, 1.0f, 0.1f, 100000.0f, "%.1f");
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["threshold"] = params_.threshold;
        json["thickness"] = params_.thickness;
        json["color"] = { params_.color[0], params_.color[1], params_.color[2], params_.color[3] };
        json["cameraNear"] = params_.cameraNear;
        json["cameraFar"] = params_.cameraFar;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.threshold = json.value("threshold", 0.1f);
        params_.thickness = json.value("thickness", 1.0f);
        if (json.contains("color") && json["color"].is_array() && json["color"].size() >= 4) {
            for (int i = 0; i < 4; ++i) params_.color[i] = json["color"][i].get<float>();
        }
        params_.cameraNear = json.value("cameraNear", 0.1f);
        params_.cameraFar = json.value("cameraFar", 1000.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.texelSize[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.texelSize[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.threshold = std::max(0.0f, params_.threshold);
        cbData_.thickness = std::max(0.0f, params_.thickness);
        for (int i = 0; i < 4; ++i) cbData_.color[i] = params_.color[i];
        cbData_.cameraNear = params_.cameraNear;
        cbData_.cameraFar = params_.cameraFar;
        PassInfo pass;
        pass.pipelineName = "PostEffect.Outline";
        pass.constantBufferRequirements = {
            { "Pixel:OutlineCB", sizeof(CBData), &cbData_ }
        };
        auto *ownerBuffer = GetOwnerScreenBuffer();
        if (ownerBuffer) {
            pass.textureBindRequirements.push_back({ "Pixel:gDepthTexture",
                [ownerBuffer]() { return ownerBuffer->GetDepthSrvHandle(); } });
        }
        pass.samplerBindRequirements.push_back({ "Pixel:gDepthSampler", DefaultSampler::PointClamp });
        return { std::move(pass) };
    }

private:
    struct CBData {
        float texelSize[2]{};
        float threshold = 0.0f;
        float thickness = 0.0f;
        float color[4]{};
        float cameraNear = 0.0f;
        float cameraFar = 0.0f;
        float pad[2]{};
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(OutlineEffect)

} // namespace KashipanEngine
