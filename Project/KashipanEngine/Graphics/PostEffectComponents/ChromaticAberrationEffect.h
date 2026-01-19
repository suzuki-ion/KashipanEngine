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

class ChromaticAberrationEffect final : public IPostEffectComponent {
public:
    struct Params {
        float directionX = 1.0f;
        float directionY = 0.0f;
        float strength = 0.0025f;
    };

    explicit ChromaticAberrationEffect(Params p = {})
        : IPostEffectComponent("ChromaticAberrationEffect", 1), params_(p) {}

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        auto c = std::make_unique<ChromaticAberrationEffect>(params_);
        return c;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Strength", &params_.strength, 0.0001f, 0.0f, 0.05f, "%.5f");
        ImGui::DragFloat2("Direction", &params_.directionX, 0.01f, -1.0f, 1.0f);
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.ChromaticAberration";
        pass.passName = "ChromaticAberration";
        pass.batchKey = 0;

        pass.constantBufferRequirements = {
            {"Pixel:ChromaticAberrationCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;
            cb->direction[0] = params_.directionX;
            cb->direction[1] = params_.directionY;
            cb->strength = params_.strength;
            cb->pad = 0.0f;
            return true;
        };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t /*instanceCount*/) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            if (!binder.Bind("Pixel:gTexture", owner->GetSrvHandle())) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", 1)) return false;
            return true;
        };

        pass.renderCommandFunction = [this](PipelineBinder &/*pb*/) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
        };

        return { std::move(pass) };
    }

private:
    struct CBData {
        float direction[2];
        float strength;
        float pad;
    };

    Params params_{};
};

} // namespace KashipanEngine
