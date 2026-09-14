#pragma once
#include <algorithm>
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief ディザパターンを使ったディゾルブ（画面フェード）ポストエフェクト
/// @details 4x4 Bayer行列の閾値をピクセルごとの「消える順番」として扱い、
///          intensityが0で無適用、1で画面全体が消えるディゾルブ表現になる
class DitherEffect final : public IPostProcessComponent {
public:
    struct Params {
        /// @brief ディゾルブ進行度（0: 無適用 〜 1: 画面全体が消える）
        float intensity = 0.0f;
        bool color = true;
        /// @brief ディザパターンの論理解像度。両方が1以上の場合のみ使用し、0なら描画先解像度を使用する
        int patternWidth = 0;
        int patternHeight = 0;
    };

    DitherEffect() : IPostProcessComponent("DitherEffect", GetComponentTypeID<DitherEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.color);
        ADD_MEMBER_VARIABLE(params_.patternWidth);
        ADD_MEMBER_VARIABLE(params_.patternHeight);
    }
    ~DitherEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<DitherEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.dithereffect.intensity"), &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::Checkbox(TranslationLabel("component.dithereffect.color"), &params_.color);
        ImGui::DragInt(TranslationLabel("component.dithereffect.pattern_width"), &params_.patternWidth, 1.0f, 0, 16384);
        ImGui::DragInt(TranslationLabel("component.dithereffect.pattern_height"), &params_.patternHeight, 1.0f, 0, 16384);
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["color"] = params_.color;
        json["patternWidth"] = params_.patternWidth;
        json["patternHeight"] = params_.patternHeight;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 0.0f);
        params_.color = json.value("color", true);
        params_.patternWidth = std::max(0, json.value("patternWidth", 0));
        params_.patternHeight = std::max(0, json.value("patternHeight", 0));
        return true;
    }

    std::vector<PassInfo> BuildPasses() override {
        auto *owner = GetOwnerScreenBuffer();
        const bool usePatternResolution = params_.patternWidth > 0 && params_.patternHeight > 0;
        const float patternWidth = usePatternResolution
            ? static_cast<float>(params_.patternWidth)
            : (owner ? static_cast<float>(owner->GetWidth()) : 0.0f);
        const float patternHeight = usePatternResolution
            ? static_cast<float>(params_.patternHeight)
            : (owner ? static_cast<float>(owner->GetHeight()) : 0.0f);
        cbData_.invPatternResolution[0] = patternWidth > 0.0f ? (1.0f / patternWidth) : 0.0f;
        cbData_.invPatternResolution[1] = patternHeight > 0.0f ? (1.0f / patternHeight) : 0.0f;
        cbData_.intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
        cbData_.color = params_.color ? 1u : 0u;
        PassInfo pass;
        pass.pipelineName = "PostEffect.Dither";
        pass.constantBufferRequirements = {
            { "Pixel:DitherCB", sizeof(CBData), &cbData_ }
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invPatternResolution[2]{};
        float intensity = 0.0f;
        unsigned int color = 0;
        unsigned int pad = 0;
    };

    Params params_{};
    CBData cbData_{};
};

REGISTER_COMPONENT_OBJECT(DitherEffect)

} // namespace KashipanEngine
