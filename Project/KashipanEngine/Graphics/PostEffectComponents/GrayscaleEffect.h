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

class GrayscaleEffect final : public IPostEffectComponent {
public:
    struct Params {
        float intensity = 1.0f;
    };

    explicit GrayscaleEffect(Params p = {})
        : IPostEffectComponent("GrayscaleEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<GrayscaleEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.Grayscale";
        pass.passName = "Grayscale";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:GrayscaleCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;
            cb->intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
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
        float pad[3];
    };

    Params params_{};
};

} // namespace KashipanEngine
