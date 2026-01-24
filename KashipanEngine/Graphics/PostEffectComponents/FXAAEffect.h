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

class FXAAEffect final : public IPostEffectComponent {
public:
    struct Params {
        float threshold = 0.01f;
        float thresholdMin = 0.001f;
        float strength = 1.0f;
    };

    explicit FXAAEffect(Params p = {})
        : IPostEffectComponent("FXAAEffect", 1), params_(p) {}

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<FXAAEffect>(params_);
    }

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Threshold", &params_.threshold, 0.001f, 0.0f, 0.1f, "%.4f");
        ImGui::DragFloat("Threshold Min", &params_.thresholdMin, 0.0001f, 0.0f, 0.01f, "%.5f");
        ImGui::DragFloat("Strength", &params_.strength, 0.1f, 0.0f, 1.0f, "%.2f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        auto *owner = GetOwnerBuffer();
        if (!owner) return {};

        std::vector<PostEffectPass> passes;
        passes.reserve(1);

        PostEffectPass pass;
        pass.pipelineName = "PostEffect.FXAA";
        pass.passName = "FXAA.Pass";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {{"Pixel:FXAACB", sizeof(CBData)}};

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;
            auto *o = GetOwnerBuffer();
            if (!o) return false;
            const float w = static_cast<float>(o->GetWidth());
            const float h = static_cast<float>(o->GetHeight());
            cb->texelSize[0] = (w > 0.0f) ? (1.0f / w) : 0.0f;
            cb->texelSize[1] = (h > 0.0f) ? (1.0f / h) : 0.0f;
            cb->threshold = std::clamp(params_.threshold, 0.0f, 1.0f);
            cb->thresholdMin = std::clamp(params_.thresholdMin, 0.0f, 1.0f);
            return true;
        };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t /*instanceCount*/) -> bool {
            auto *o = GetOwnerBuffer();
            if (!o) return false;
            if (!binder.Bind("Pixel:gTexture", o->GetSrvHandle())) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
            return true;
        };

        pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
        };

        passes.push_back(std::move(pass));
        return passes;
    }

private:
    struct CBData {
        float texelSize[2];
        float threshold;
        float thresholdMin;
        float strength;
    };

    Params params_{};
};

} // namespace KashipanEngine
