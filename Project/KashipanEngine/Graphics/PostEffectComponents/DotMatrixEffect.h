#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class DotMatrixEffect final : public IPostEffectComponent {
public:
    struct Params {
        float dotSpacing = 8.0f;    // ドット間隔（ピクセル）
        float dotRadius = 3.5f;     // ドット半径（ピクセル）
        float threshold = 0.0f;     // 輝度閾値（0..1）、0で無効
        float intensity = 1.0f;     // 出力強度（乗算）
        bool  monochrome = false;   // 単色化するか
    };

    explicit DotMatrixEffect(Params p = {})
        : IPostEffectComponent("DotMatrixEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<DotMatrixEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("DotSpacing", &params_.dotSpacing, 0.25f, 1.0f, 256.0f);
        ImGui::DragFloat("DotRadius", &params_.dotRadius, 0.125f, 0.0f, 256.0f);
        ImGui::DragFloat("Threshold", &params_.threshold, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 4.0f);
        ImGui::Checkbox("Monochrome", &params_.monochrome);
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.DotMatrix";
        pass.passName = "DotMatrix";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:DotMatrixCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            auto *owner = GetOwnerBuffer();
            if (!owner) return false;

            cb->invResolution[0] = (owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
            cb->invResolution[1] = (owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
            cb->dotSpacing = params_.dotSpacing;
            cb->dotRadius = params_.dotRadius;
            cb->threshold = params_.threshold;
            cb->intensity = params_.intensity;
            cb->monochrome = params_.monochrome ? 1u : 0u;
            cb->pad = 0u;
            return true;
        };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t /*instanceCount*/) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            if (!binder.Bind("Pixel:gTexture", owner->GetSrvHandle())) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", 1)) return false;
            return true;
        };

        pass.renderCommandFunction = [](PipelineBinder &/*pb*/) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float invResolution[2];
        float dotSpacing;
        float dotRadius;
        float threshold;
        float intensity;
        unsigned int monochrome;
        unsigned int pad;
    };

    Params params_{};
};

} // namespace KashipanEngine
