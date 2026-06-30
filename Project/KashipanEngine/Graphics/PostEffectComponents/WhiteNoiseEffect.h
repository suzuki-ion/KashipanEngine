#pragma once
#include <memory>
#include <vector>
#include <string>
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class WhiteNoiseEffect final : public IPostEffectComponent {
public:
    struct Params {
        float noiseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        uint32_t noiseSeed = 0;
        float noiseIntensity = 1.0f;
        float noiseScale = 1.0f;
        float noiseTime = 0.0f;
        bool isAutoUpdateTime = true;
    };
    explicit WhiteNoiseEffect(Params p = {})
        : IPostEffectComponent("WhiteNoiseEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<WhiteNoiseEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::ColorEdit4("Noise Color", params_.noiseColor);
        ImGui::DragInt("Noise Seed", reinterpret_cast<int *>(&params_.noiseSeed), 1, 0, INT_MAX);
        ImGui::DragFloat("Noise Intensity", &params_.noiseIntensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Noise Scale", &params_.noiseScale, 0.01f, 0.0f, 10.0f, "%.3f");
        ImGui::DragFloat("Noise Time", &params_.noiseTime, 0.01f, 0.0f, 100.0f, "%.3f");
        ImGui::Checkbox("Auto Update Time", &params_.isAutoUpdateTime);
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.WhiteNoise";
        pass.passName = "WhiteNoise";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:WhiteNoiseCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;

            auto *owner = GetOwnerBuffer();
            if (!owner) return false;

            cb->noiseColor[0] = params_.noiseColor[0];
            cb->noiseColor[1] = params_.noiseColor[1];
            cb->noiseColor[2] = params_.noiseColor[2];
            cb->noiseColor[3] = params_.noiseColor[3];
            cb->noiseSeed = params_.noiseSeed;
            cb->noiseIntensity = params_.noiseIntensity;
            cb->noiseScale = params_.noiseScale;
            if (params_.isAutoUpdateTime) params_.noiseTime += 0.01f;
            cb->noiseTime = params_.noiseTime;
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
        float noiseColor[4];
        uint32_t noiseSeed;
        float noiseIntensity;
        float noiseScale;
        float noiseTime;
    };

    Params params_{};
};

} // namespace KashipanEngine