#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"

namespace KashipanEngine {

/// @brief FXAA（アンチエイリアス）ポストエフェクト
class FXAAEffect final : public IPostProcessComponent {
public:
    struct Params {
        float threshold = 0.01f;
        float thresholdMin = 0.001f;
        float strength = 1.0f;
    };

    FXAAEffect() : IPostProcessComponent("FXAAEffect") {
        ADD_MEMBER_VARIABLE(params_.threshold);
        ADD_MEMBER_VARIABLE(params_.thresholdMin);
        ADD_MEMBER_VARIABLE(params_.strength);
    }
    ~FXAAEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<FXAAEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Threshold", &params_.threshold, 0.001f, 0.0f, 0.5f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("エッジ検出の絶対しきい値。実用域はおよそ0.03〜0.16で、\nそれ以上はほぼ全てのエッジが無視されAAが効かなくなる");
        }
        ImGui::DragFloat("Threshold Min", &params_.thresholdMin, 0.0001f, 0.0f, 0.2f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("周囲の最大輝度に対する相対しきい値。実用域はおよそ0.03〜0.08");
        }
        ImGui::DragFloat("Strength", &params_.strength, 0.01f, 0.0f, 1.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("AA適用後の色への合成比率（0=無効、1=適用後の色をそのまま採用）");
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["threshold"] = params_.threshold;
        json["thresholdMin"] = params_.thresholdMin;
        json["strength"] = params_.strength;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.threshold = json.value("threshold", 0.01f);
        params_.thresholdMin = json.value("thresholdMin", 0.001f);
        params_.strength = json.value("strength", 1.0f);
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        cbData_.texelSize[0] = (owner && owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
        cbData_.texelSize[1] = (owner && owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
        cbData_.threshold = std::max(params_.threshold, 0.0f);
        cbData_.thresholdMin = std::max(params_.thresholdMin, 0.0f);
        // 1.0を超えるとlerpが外挿されFXAA自身のluma範囲チェックを飛び越えて色が破綻するため上限も設ける
        cbData_.strength = std::clamp(params_.strength, 0.0f, 1.0f);
        PassInfo pass;
        pass.pipelineName = "PostEffect.FXAA";
        pass.constantBufferRequirements = {
            { "Pixel:FXAACB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float texelSize[2]{};
        float threshold = 0.0f;
        float thresholdMin = 0.0f;
        float strength = 0.0f;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(FXAAEffect)

} // namespace KashipanEngine
