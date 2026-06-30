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

class OutlineEffect final : public IPostEffectComponent {
public:
    struct Params {
        float threshold = 0.1f; // 輪郭検出の深度差の閾値
        float thickness = 1.0f; // 輪郭の太さ（ピクセル）
        float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // 輪郭の色 (RGBA)
        float cameraNear = 0.1f; // カメラのニアクリップ距離（深度値の線形化に使用）
        float cameraFar = 1000.0f; // カメラのファークリップ距離（深度値の線形化に使用）
    };

    explicit OutlineEffect(Params p = {})
        : IPostEffectComponent("OutlineEffect", 1), params_(p) {}
    
    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }
    
    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<OutlineEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Threshold", &params_.threshold, 0.001f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Thickness", &params_.thickness, 0.1f, 0.0f, 10.0f, "%.1f");
        ImGui::ColorEdit4("Color", params_.color);
    }
#endif
    std::vector<PostEffectPass> BuildPostEffectPasses() override {
        PostEffectPass pass;
        pass.pipelineName = "PostEffect.Outline";
        pass.passName = "Outline";
        pass.batchKey = 0;
        
        pass.constantBufferRequirements = {
            {"Pixel:OutlineCB", sizeof(CBData)}
        };

        pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            cb->texelSize[0] = (owner->GetWidth() > 0) ? (1.0f / static_cast<float>(owner->GetWidth())) : 0.0f;
            cb->texelSize[1] = (owner->GetHeight() > 0) ? (1.0f / static_cast<float>(owner->GetHeight())) : 0.0f;
            cb->threshold = params_.threshold;
            cb->thickness = params_.thickness;
            cb->color[0] = params_.color[0];
            cb->color[1] = params_.color[1];
            cb->color[2] = params_.color[2];
            cb->color[3] = params_.color[3];
            cb->cameraNear = params_.cameraNear;
            cb->cameraFar = params_.cameraFar;
            cb->pad[0] = 0.0f;
            cb->pad[1] = 0.0f;
            return true;
            };

        pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
            auto *owner = GetOwnerBuffer();
            if (!owner) return false;
            if (!binder.Bind("Pixel:gTexture", owner->GetSrvHandle())) return false;
            if (!binder.Bind("Pixel:gDepthTexture", owner->GetDepthSrvHandle())) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
            if (!SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp)) return false;
            return true;
            };

        pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
            return MakeDrawCommand(3);
            };

        return { pass };
    }

private:
    struct CBData {
        float texelSize[2]; // 1.0f / 解像度
        float threshold; // 輪郭検出の深度差の閾値
        float thickness; // 輪郭の太さ
        float color[4]; // 輪郭の色 (RGBA)
        float cameraNear; // カメラのニアクリップ距離
        float cameraFar; // カメラのファークリップ距離
        float pad[2]; // パディング
    };

    Params params_;
};

} // namespace KashipanEngine