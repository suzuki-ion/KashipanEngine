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

class BoxFilterEffect final : public IPostEffectComponent {
public:
    struct Params {
        float intensity = 1.0f;
        int halfSize[2] = { 2, 2 };
    };

    explicit BoxFilterEffect(Params p = {})
        : IPostEffectComponent("BoxFilterEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<BoxFilterEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragInt2("Half Size", params_.halfSize, 1.0f, 1);
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.BoxFilter";
        pass.passName = "BoxFilter";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:BoxFilterCB", sizeof(CBData)}
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
            cb->intensity = std::clamp(params_.intensity, 0.0f, 1.0f);
            cb->halfSize[0] = params_.halfSize[0];
            cb->halfSize[1] = params_.halfSize[1];
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
        float intensity;
        float pad;
        int halfSize[2];
    };

    Params params_{};
};

} // namespace KashipanEngine
