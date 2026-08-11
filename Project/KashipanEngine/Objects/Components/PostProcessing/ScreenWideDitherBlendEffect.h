#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>

#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 画面全体（影を含む）を位相違いでNパス再描画し、結果を平均してディザの粒状感を
///        1フレーム内で滑らかにする設定コンポーネント
/// @details オブジェクト単位のenableMultiPassDither（Renderer::RenderMultiPassDither）とは異なり、
///          対象を特定のマテリアルに限らず、シャドウマップ生成を含む描画先全体を丸ごとNパス
///          描画してブレンドする。実際の描画処理はRenderer::RenderFrameが、このコンポーネントの
///          有無・パス数をRenderShadowMaps/RenderToTargetより前に検査して直接実装する
///          （BuildPasses/RenderCustomによる通常のポストエフェクトパイプラインは経由しない。
///          シャドウマップの再生成を伴うため）。そのためこのクラス自体は設定値（パス数）のみを
///          保持し、GPU処理を持たない。付与するとその描画先の描画コストがほぼパス数倍になるため、
///          常時有効にする用途ではなく、スクリーンショットや一時停止中の高品質表示等での
///          利用を想定している（ShowImGuiに注意書きを表示する）
class ScreenWideDitherBlendEffect final : public IPostProcessComponent {
public:
    struct Params {
        /// @brief 影も含めて画面全体を撮り直す回数N（1〜8にクランプして使用）
        std::uint32_t passCount = 4;
    };

    ScreenWideDitherBlendEffect()
        : IPostProcessComponent("ScreenWideDitherBlendEffect", GetComponentTypeID<ScreenWideDitherBlendEffect>()) {
        ADD_MEMBER_VARIABLE(params_.passCount);
    }
    ~ScreenWideDitherBlendEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<ScreenWideDitherBlendEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    /// @brief パス数N（1〜8にクランプ済み）を取得
    std::uint32_t GetPassCount() const noexcept { return std::clamp(params_.passCount, 1u, 8u); }
    void SetPassCount(std::uint32_t count) { params_.passCount = std::clamp(count, 1u, 8u); }

protected:
#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        int passCount = static_cast<int>(params_.passCount);
        if (ImGui::SliderInt(TranslationLabel("component.screenwideditherblendeffect.pass_count"), &passCount, 1, 8)) {
            params_.passCount = static_cast<std::uint32_t>(std::clamp(passCount, 1, 8));
        }
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "%s", TranslationC("component.screenwideditherblendeffect.warning"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.screenwideditherblendeffect.warning_detail"));
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["passCount"] = params_.passCount;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.passCount = json.value("passCount", 4u);
        return true;
    }

    /// @brief Renderer::RenderFrameが直接処理するため、通常のポストエフェクトパスは使わない
    std::vector<PassInfo> BuildPasses() override { return {}; }

private:
    Params params_{};
};

REGISTER_COMPONENT_OBJECT(ScreenWideDitherBlendEffect)

} // namespace KashipanEngine
