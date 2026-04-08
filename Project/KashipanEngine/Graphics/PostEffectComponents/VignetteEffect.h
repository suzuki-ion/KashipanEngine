#pragma once
#pragma once

#include <algorithm>
#include <memory>
#include <vector>

#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class VignetteEffect final : public IPostEffectComponent {
public:
    struct Params {
        float center[2] = {0.5f, 0.5f};
        Vector4 color = Vector4{0.0f, 0.0f, 0.0f, 1.0f};
        float intensity = 0.0f;
        float innerRadius = 0.3f;
        float smoothness = 0.3f;
    };

    explicit VignetteEffect(Params p = {})
        : IPostEffectComponent("VignetteEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<VignetteEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat2("Center", params_.center, 0.001f, -2.0f, 2.0f, "%.3f");
        ImGui::ColorEdit4("Color", &params_.color.x);
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Inner Radius", &params_.innerRadius, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Smoothness", &params_.smoothness, 0.001f, 0.0001f, 2.0f, "%.3f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.Vignette";
        pass.passName = "Vignette";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:VignetteCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            cb->center[0] = params_.center[0];
            cb->center[1] = params_.center[1];
            cb->color = params_.color;
            cb->intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
            cb->innerRadius = std::clamp(params_.innerRadius, 0.0f, 1.0f);
            cb->smoothness = std::max(0.0001f, params_.smoothness);
            cb->pad0[0] = 0.0f;
            cb->pad0[1] = 0.0f;
            cb->pad1 = 0.0f;
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

        return {std::move(pass)};
    }

private:
    struct CBData {
        float center[2];
        float pad0[2];
        Vector4 color;
        float intensity;
        float innerRadius;
        float smoothness;
        float pad1;
    };

    Params params_{};
};

} // namespace KashipanEngine
