#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class RadialBlurEffect final : public IPostEffectComponent {
public:
    struct Params {
        float intensity = 0.25f;
        int sampleCount = 8;
        float radialCenter[2] = { 0.5f, 0.5f };
        float startRadius = 0.0f;
    };

    explicit RadialBlurEffect(Params p = {})
        : IPostEffectComponent("RadialBlurEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<RadialBlurEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 2.0f, "%.3f");
        ImGui::DragInt("Sample Count", &params_.sampleCount, 1.0f, 1, 64);
        ImGui::DragFloat2("Radial Center", params_.radialCenter, 0.001f, -1.0f, 2.0f, "%.3f");
        ImGui::DragFloat("Start Radius", &params_.startRadius, 0.001f, 0.0f, 1.0f, "%.3f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.RadialBlur";
        pass.passName = "RadialBlur";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:RadialBlurCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            cb->intensity = std::max(params_.intensity, 0.0f);
            cb->sampleCount = static_cast<std::uint32_t>(std::clamp(params_.sampleCount, 1, 64));
            cb->radialCenter[0] = params_.radialCenter[0];
            cb->radialCenter[1] = params_.radialCenter[1];
            cb->startRadius = std::clamp(params_.startRadius, 0.0f, 1.0f);
            cb->pad[0] = 0.0f;
            cb->pad[1] = 0.0f;
            cb->pad[2] = 0.0f;
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
        float intensity;
        std::uint32_t sampleCount;
        float radialCenter[2];
        float startRadius;
        float pad[3];
    };

    Params params_{};
};

} // namespace KashipanEngine
