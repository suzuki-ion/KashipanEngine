#pragma once
#include <algorithm>
#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 輪郭線ポストエフェクト
class OutlineEffect final : public IPostProcessComponent {
public:
    struct Params {
        float threshold = 0.1f;
        float thickness = 1.0f;
        float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    OutlineEffect() : IPostProcessComponent("OutlineEffect", GetComponentTypeID<OutlineEffect>()) {
        ADD_MEMBER_VARIABLE(params_.threshold);
        ADD_MEMBER_VARIABLE(params_.thickness);
        AddMemberVariable("params_.color[0]", &params_.color[0]);
        AddMemberVariable("params_.color[1]", &params_.color[1]);
        AddMemberVariable("params_.color[2]", &params_.color[2]);
        AddMemberVariable("params_.color[3]", &params_.color[3]);
    }
    ~OutlineEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<OutlineEffect>();
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
        ImGui::DragFloat(TranslationLabel("component.outlineeffect.threshold"), &params_.threshold, 0.001f, 0.0f, 10.0f, "%.4f");
        ImGui::DragFloat(TranslationLabel("component.outlineeffect.thickness"), &params_.thickness, 0.1f, 0.0f, 16.0f, "%.1f");
        ImGui::ColorEdit4(TranslationLabel("component.outlineeffect.color"), params_.color);
        if (!GetCameraInfo().valid) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                "No camera resolved for this ScreenBuffer yet: depth linearization may be inaccurate.");
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["threshold"] = params_.threshold;
        json["thickness"] = params_.thickness;
        json["color"] = { params_.color[0], params_.color[1], params_.color[2], params_.color[3] };
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.threshold = json.value("threshold", 0.1f);
        params_.thickness = json.value("thickness", 1.0f);
        if (json.contains("color") && json["color"].is_array() && json["color"].size() >= 4) {
            for (int i = 0; i < 4; ++i) params_.color[i] = json["color"][i].get<float>();
        }
        // 旧バージョンで保存されたcameraNear/cameraFarが残っていても、以後はカメラから自動解決するため無視する
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        LogScope scope;
        auto *owner = GetOwnerScreenBuffer();
        cbData_.texelSize[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.texelSize[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.threshold = std::max(0.0f, params_.threshold);
        cbData_.thickness = std::max(0.0f, params_.thickness);
        for (int i = 0; i < 4; ++i) cbData_.color[i] = params_.color[i];
        // カメラのNear/FarはユーザーにOutline側で複製させず、実際に描画したカメラから自動解決する
        const CameraInfo &cameraInfo = GetCameraInfo();
        cbData_.cameraNear = cameraInfo.nearClip;
        cbData_.cameraFar = cameraInfo.farClip;
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
