#pragma once
#include <algorithm>
#include "Math/Vector4.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 樽型歪み(ブラウン管風の画面湾曲)ポストエフェクト
class BarrelDistortionEffect final : public IPostProcessComponent {
public:
    struct Params {
        float strength = 0.15f; // 正の値で樽型(ブラウン管風)、負の値で枕型に歪む
        float zoom = 1.0f;
        Vector4 edgeColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    };

    BarrelDistortionEffect() : IPostProcessComponent("BarrelDistortionEffect", GetComponentTypeID<BarrelDistortionEffect>()) {
        ADD_MEMBER_VARIABLE(params_.strength);
        ADD_MEMBER_VARIABLE(params_.zoom);
        ADD_MEMBER_VARIABLE(params_.edgeColor);
    }
    ~BarrelDistortionEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<BarrelDistortionEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.barreldistortioneffect.strength"), &params_.strength, 0.005f, -1.0f, 1.0f, "%.3f");
        ImGui::DragFloat(TranslationLabel("component.barreldistortioneffect.zoom"), &params_.zoom, 0.005f, 0.5f, 2.0f, "%.3f");
        ImGui::ColorEdit4(TranslationLabel("component.barreldistortioneffect.edge_color"), &params_.edgeColor.x);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["strength"] = params_.strength;
        json["zoom"] = params_.zoom;
        json["edgeColor"] = ToJSON(params_.edgeColor);
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.strength = json.value("strength", 0.15f);
        params_.zoom = json.value("zoom", 1.0f);
        if (json.contains("edgeColor")) params_.edgeColor = FromJSON<Vector4>(json["edgeColor"]);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        cbData_.strength = params_.strength;
        cbData_.zoom = std::max(0.0001f, params_.zoom);
        cbData_.edgeColor = params_.edgeColor;
        PassInfo pass;
        pass.pipelineName = "PostEffect.BarrelDistortion";
        pass.constantBufferRequirements = {
            { "Pixel:BarrelDistortionCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float strength = 0.0f;
        float zoom = 0.0f;
        float pad0[2]{};
        Vector4 edgeColor;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(BarrelDistortionEffect)

} // namespace KashipanEngine
