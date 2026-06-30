#pragma once
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class ColorAdjustEffect final : public IPostEffectComponent {
public:
    struct Params {
        float brightness = 0.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;
        float temperature = 0.0f;
        float colorBalance[3] = {0.0f, 0.0f, 0.0f};
    };

    explicit ColorAdjustEffect(Params p = {})
        : IPostEffectComponent("ColorAdjustEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<ColorAdjustEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Brightness", &params_.brightness, 0.01f, -1.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Contrast", &params_.contrast, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::DragFloat("Saturation", &params_.saturation, 0.01f, 0.0f, 4.0f, "%.3f");
        ImGui::DragFloat("Temperature", &params_.temperature, 0.01f, -2.0f, 2.0f, "%.3f");
        ImGui::DragFloat3("ColorBalance", params_.colorBalance, 0.01f, -1.0f, 1.0f, "%.3f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.ColorAdjust";
        pass.passName = "ColorAdjust";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:ColorAdjustCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            cb->brightness = std::clamp(params_.brightness, -1.0f, 1.0f);
            cb->contrast = std::max(params_.contrast, 0.0f);
            cb->saturation = std::max(params_.saturation, 0.0f);
            cb->temperature = std::clamp(params_.temperature, -1.0f, 1.0f);
            cb->colorBalance[0] = std::clamp(params_.colorBalance[0], -1.0f, 1.0f);
            cb->colorBalance[1] = std::clamp(params_.colorBalance[1], -1.0f, 1.0f);
            cb->colorBalance[2] = std::clamp(params_.colorBalance[2], -1.0f, 1.0f);
            cb->pad = 0.0f;
            return true;
        };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            if (!binder.Bind("Pixel:gTexture", owner->GetSrvHandle())) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
            return true;
        };

        pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float brightness;
        float contrast;
        float saturation;
        float temperature;
        float colorBalance[3];
        float pad;
    };

    Params params_{};
};

} // namespace KashipanEngine
