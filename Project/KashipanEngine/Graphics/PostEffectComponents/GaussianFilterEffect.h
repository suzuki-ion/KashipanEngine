#pragma once
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>

#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class GaussianFilterEffect final : public IPostEffectComponent {
public:
    struct Params {
        int radius = 3; // サンプル半径
        float sigma = 1.0f;
    };

    explicit GaussianFilterEffect(Params p = {})
        : IPostEffectComponent("GaussianFilterEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<GaussianFilterEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragInt("Radius", &params_.radius, 1.0f, 0, 32);
        ImGui::DragFloat("Sigma", &params_.sigma, 0.01f, 0.01f, 32.0f, "%.3f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.GaussianFilter";
        pass.passName = "GaussianFilter";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:GaussianCB", sizeof(CBData)}
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
            cb->radius = std::clamp(params_.radius, 0, 32);
            cb->sigma = std::max(params_.sigma, 0.0001f);
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
        float invResolution[2];
        int radius;
        float sigma;
        float pad;
    };

    Params params_{};
};

} // namespace KashipanEngine
