#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <memory>
#include <string>

#include "Debug/Logger.h"
#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/ScreenBuffer.h"
#include "Utilities/Translation.h"

namespace KashipanEngine {

/// @brief 半解像度で計算するGround-Truth Ambient Occlusionポストエフェクト
/// @details 深度から位置と法線を再構成し、複数の画面スライス上で地平線角を探索する。
///          時間蓄積には依存せず、半解像度の横・縦バイラテラルフィルターと
///          深度対応アップサンプルによって低サンプル時のノイズと輪郭のにじみを抑える。
class GTAOEffect final : public IPostProcessComponent {
public:
    struct Params {
        float radius = 0.5f;
        float intensity = 1.0f;
        float power = 1.0f;
        float bias = 0.02f;
        std::uint32_t directionCount = 4;
        std::uint32_t stepCount = 6;
        int blurRadius = 3;
        float depthThreshold = 0.5f;
        bool showAOOnly = false;
    };

    GTAOEffect() : IPostProcessComponent("GTAOEffect", GetComponentTypeID<GTAOEffect>()) {
        ADD_MEMBER_VARIABLE(params_.radius);
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.power);
        ADD_MEMBER_VARIABLE(params_.bias);
        ADD_MEMBER_VARIABLE(params_.directionCount);
        ADD_MEMBER_VARIABLE(params_.stepCount);
        ADD_MEMBER_VARIABLE(params_.blurRadius);
        ADD_MEMBER_VARIABLE(params_.depthThreshold);
        ADD_MEMBER_VARIABLE(params_.showAOOnly);
    }
    ~GTAOEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        LogScope scope;
        auto ptr = std::make_unique<GTAOEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) {
        LogScope scope;
        params_ = params;
        SanitizeParams();
    }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        LogScope scope;
        IPostProcessComponent::Finalize();
        aoRaw_ = {};
        aoBlurTemp_ = {};
        aoBlurred_ = {};
        gtaoConstantBuffer_.reset();
        blurConstantBuffers_[0].reset();
        blurConstantBuffers_[1].reset();
        compositeConstantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        LogScope scope;
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.gtaoeffect.radius"), &params_.radius, 0.01f, 0.01f, 20.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_radius"));
        }
        ImGui::DragFloat(TranslationLabel("component.gtaoeffect.intensity"), &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_intensity"));
        }
        ImGui::DragFloat(TranslationLabel("component.gtaoeffect.power"), &params_.power, 0.01f, 0.1f, 8.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_power"));
        }
        ImGui::DragFloat(TranslationLabel("component.gtaoeffect.bias"), &params_.bias, 0.001f, 0.0f, 1.0f, "%.4f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_bias"));
        }

        int directionCount = static_cast<int>(params_.directionCount);
        if (ImGui::DragInt(TranslationLabel("component.gtaoeffect.direction_count"), &directionCount, 1.0f, 2, 8)) {
            params_.directionCount = static_cast<std::uint32_t>(std::clamp(directionCount, 2, 8));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_direction_count"));
        }
        int stepCount = static_cast<int>(params_.stepCount);
        if (ImGui::DragInt(TranslationLabel("component.gtaoeffect.step_count"), &stepCount, 1.0f, 2, 12)) {
            params_.stepCount = static_cast<std::uint32_t>(std::clamp(stepCount, 2, 12));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_step_count"));
        }
        ImGui::DragInt(TranslationLabel("component.gtaoeffect.blur_radius"), &params_.blurRadius, 1.0f, 0, 8);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_blur_radius"));
        }
        ImGui::DragFloat(TranslationLabel("component.gtaoeffect.depth_threshold"), &params_.depthThreshold, 0.01f, 0.001f, 100.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_depth_threshold"));
        }
        ImGui::Checkbox(TranslationLabel("component.gtaoeffect.show_ao_only"), &params_.showAOOnly);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TranslationC("component.gtaoeffect.desc_show_ao_only"));
        }
        SanitizeParams();

        if (!lastCameraValid_) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                "No camera resolved for this ScreenBuffer yet: GTAO will not be applied.");
        }
    }
#endif

    JSON SaveToJson() const override {
        LogScope scope;
        JSON json = IPostProcessComponent::SaveToJson();
        json["radius"] = params_.radius;
        json["intensity"] = params_.intensity;
        json["power"] = params_.power;
        json["bias"] = params_.bias;
        json["directionCount"] = params_.directionCount;
        json["stepCount"] = params_.stepCount;
        json["blurRadius"] = params_.blurRadius;
        json["depthThreshold"] = params_.depthThreshold;
        json["showAOOnly"] = params_.showAOOnly;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        LogScope scope;
        IPostProcessComponent::LoadFromJson(json);
        params_.radius = json.value("radius", 0.5f);
        params_.intensity = json.value("intensity", 1.0f);
        params_.power = json.value("power", 1.0f);
        params_.bias = json.value("bias", 0.02f);
        params_.directionCount = json.value("directionCount", 4u);
        params_.stepCount = json.value("stepCount", 6u);
        params_.blurRadius = json.value("blurRadius", 3);
        params_.depthThreshold = json.value("depthThreshold", 0.5f);
        params_.showAOOnly = json.value("showAOOnly", false);
        SanitizeParams();
        return true;
    }

    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        LogScope scope;
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        const CameraInfo &cameraInfo = GetCameraInfo();
        lastCameraValid_ = cameraInfo.valid;
        if (!cameraInfo.valid) return false;

        static const char *kGTAO = "PostEffect.GTAO";
        static const char *kBlur = "PostEffect.GTAO.Blur";
        static const char *kComposite = "PostEffect.GTAO.Composite";
        if (!context.pipelineManager->HasPipeline(kGTAO) ||
            !context.pipelineManager->HasPipeline(kBlur) ||
            !context.pipelineManager->HasPipeline(kComposite)) {
            return false;
        }
        if (!EnsureIntermediateTargets(screenBuffer)) return false;

        screenBuffer->NextPass();

        auto drawTo = [&](EffectRT &dst, const char *pipelineName, auto &&bindFunc) {
            dst.rt->SetCommandList(commandList);
            if (!dst.rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return;

            const auto rtv = dst.rt->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            D3D12_VIEWPORT viewport{};
            viewport.Width = static_cast<float>(dst.width);
            viewport.Height = static_cast<float>(dst.height);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            D3D12_RECT scissor{};
            scissor.right = static_cast<LONG>(dst.width);
            scissor.bottom = static_cast<LONG>(dst.height);
            commandList->RSSetViewports(1, &viewport);
            commandList->RSSetScissorRects(1, &scissor);

            context.pipelineBinder->UsePipeline(pipelineName);
            auto &binder = context.getShaderBinder(pipelineName);
            bindFunc(binder);
            commandList->DrawInstanced(3, 1, 0, 0);
            dst.rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        };

        {
            GTAOCBData cbData{};
            cbData.viewProjection = cameraInfo.viewProjection;
            cbData.invViewProjection = cameraInfo.viewProjection.Inverse();
            cbData.cameraWorldPosition = cameraInfo.worldPosition;
            cbData.radius = params_.radius;
            cbData.intensity = params_.intensity;
            cbData.power = params_.power;
            cbData.bias = params_.bias;
            cbData.depthThreshold = params_.depthThreshold;
            cbData.directionCount = params_.directionCount;
            cbData.stepCount = params_.stepCount;
            cbData.depthTexelSize[0] = 1.0f / static_cast<float>(screenBuffer->GetWidth());
            cbData.depthTexelSize[1] = 1.0f / static_cast<float>(screenBuffer->GetHeight());
            cbData.aoTexelSize[0] = 1.0f / static_cast<float>(aoRaw_.width);
            cbData.aoTexelSize[1] = 1.0f / static_cast<float>(aoRaw_.height);
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            if (!gtaoConstantBuffer_) gtaoConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(GTAOCBData));
            if (void *mapped = gtaoConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(aoRaw_, kGTAO, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:GTAOCB", gtaoConstantBuffer_.get());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        }

        auto drawBlurPass = [&](EffectRT &dst, EffectRT &src, std::size_t index, float directionX, float directionY) {
            GTAOBlurCBData cbData{};
            cbData.aoTexelSize[0] = 1.0f / static_cast<float>(aoRaw_.width);
            cbData.aoTexelSize[1] = 1.0f / static_cast<float>(aoRaw_.height);
            cbData.direction[0] = directionX;
            cbData.direction[1] = directionY;
            cbData.radius = params_.blurRadius;
            cbData.depthThreshold = params_.depthThreshold;
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            if (!blurConstantBuffers_[index]) blurConstantBuffers_[index] = std::make_unique<ConstantBufferResource>(sizeof(GTAOBlurCBData));
            if (void *mapped = blurConstantBuffers_[index]->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(dst, kBlur, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:GTAOBlurCB", blurConstantBuffers_[index].get());
                binder.Bind("Pixel:gAOTexture", src.srv->GetGPUDescriptorHandle());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gAOSampler", DefaultSampler::PointClamp);
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        };
        drawBlurPass(aoBlurTemp_, aoRaw_, 0, 1.0f, 0.0f);
        drawBlurPass(aoBlurred_, aoBlurTemp_, 1, 0.0f, 1.0f);

        {
            GTAOCompositeCBData cbData{};
            cbData.aoTexelSize[0] = 1.0f / static_cast<float>(aoBlurred_.width);
            cbData.aoTexelSize[1] = 1.0f / static_cast<float>(aoBlurred_.height);
            cbData.depthThreshold = params_.depthThreshold;
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            cbData.showAOOnly = params_.showAOOnly ? 1u : 0u;
            if (!compositeConstantBuffer_) compositeConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(GTAOCompositeCBData));
            if (void *mapped = compositeConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            screenBuffer->RebindWriteTarget();
            context.pipelineBinder->UsePipeline(kComposite);
            auto &binder = context.getShaderBinder(kComposite);
            binder.Bind("Pixel:GTAOCompositeCB", compositeConstantBuffer_.get());
            binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
            binder.Bind("Pixel:gAOTexture", aoBlurred_.srv->GetGPUDescriptorHandle());
            binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSceneSampler", DefaultSampler::LinearClamp);
            SamplerManager::BindSampler(&binder, "Pixel:gAOSampler", DefaultSampler::PointClamp);
            SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            commandList->DrawInstanced(3, 1, 0, 0);
        }

        return true;
    }

private:
    struct GTAOCBData {
        Matrix4x4 viewProjection = Matrix4x4::Identity();
        Matrix4x4 invViewProjection = Matrix4x4::Identity();
        Vector3 cameraWorldPosition{ 0.0f, 0.0f, 0.0f };
        float radius = 0.5f;
        float intensity = 1.0f;
        float power = 1.0f;
        float bias = 0.02f;
        float depthThreshold = 0.5f;
        std::uint32_t directionCount = 4;
        std::uint32_t stepCount = 6;
        float depthTexelSize[2]{};
        float aoTexelSize[2]{};
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };

    struct GTAOBlurCBData {
        float aoTexelSize[2]{};
        float direction[2]{};
        int radius = 3;
        float depthThreshold = 0.5f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };

    struct GTAOCompositeCBData {
        float aoTexelSize[2]{};
        float depthThreshold = 0.5f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        std::uint32_t showAOOnly = 0;
        float pad[2]{};
    };

    struct EffectRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    void SanitizeParams() {
        LogScope scope;
        params_.radius = std::max(params_.radius, 0.01f);
        params_.intensity = std::max(params_.intensity, 0.0f);
        params_.power = std::max(params_.power, 0.1f);
        params_.bias = std::max(params_.bias, 0.0f);
        params_.directionCount = std::clamp(params_.directionCount, 2u, 8u);
        params_.stepCount = std::clamp(params_.stepCount, 2u, 12u);
        params_.blurRadius = std::clamp(params_.blurRadius, 0, 8);
        params_.depthThreshold = std::max(params_.depthThreshold, 0.001f);
    }

    bool EnsureIntermediateTargets(ScreenBuffer *owner) {
        LogScope scope;
        const std::uint32_t width = (owner->GetWidth() + 1u) / 2u;
        const std::uint32_t height = (owner->GetHeight() + 1u) / 2u;
        if (width == 0 || height == 0) return false;

        constexpr DXGI_FORMAT kAOFormat = DXGI_FORMAT_R8_UNORM;
        auto ensure = [&](EffectRT &target) {
            if (!target.rt || !target.srv || target.width != width || target.height != height) {
                target.rt = std::make_unique<RenderTargetResource>(width, height, kAOFormat);
                target.srv = std::make_unique<ShaderResourceResource>(target.rt.get());
                target.width = width;
                target.height = height;
            }
        };
        ensure(aoRaw_);
        ensure(aoBlurTemp_);
        ensure(aoBlurred_);
        return aoRaw_.rt && aoRaw_.srv && aoBlurTemp_.rt && aoBlurTemp_.srv && aoBlurred_.rt && aoBlurred_.srv;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> gtaoConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> blurConstantBuffers_[2];
    std::unique_ptr<ConstantBufferResource> compositeConstantBuffer_;
    EffectRT aoRaw_;
    EffectRT aoBlurTemp_;
    EffectRT aoBlurred_;
    bool lastCameraValid_ = true;
};

REGISTER_COMPONENT_OBJECT(GTAOEffect)

} // namespace KashipanEngine
