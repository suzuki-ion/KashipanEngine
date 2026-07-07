#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <memory>
#include <string>
#include <vector>

#include "Objects/Components/PostProcessing/IPostProcessComponent.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Resources/ConstantBufferResource.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"
#include "Graphics/ScreenBuffer.h"

namespace KashipanEngine {

/// @brief ブルームポストエフェクト
/// @details 中間レンダーターゲット（縮小ピラミッド）を使う多段パスのため、
///          宣言的なパス定義ではなくカスタム描画で実装している。
class BloomEffect final : public IPostProcessComponent {
public:
    struct Params {
        float threshold = 1.0f;
        float softKnee = 0.5f;
        float intensity = 0.8f;
        float blurRadius = 1.0f;
        std::uint32_t iterations = 4; // ダウンサンプル段数 (1..16)
    };

    BloomEffect() : IPostProcessComponent("BloomEffect") {}
    ~BloomEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<BloomEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        pyramid_.clear();
        pyramidBlur_.clear();
        pyramidAccum_.clear();
        constantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Threshold", &params_.threshold, 0.01f, 0.0f, 10.0f, "%.3f");
        ImGui::DragFloat("SoftKnee", &params_.softKnee, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGui::DragFloat("BlurRadius", &params_.blurRadius, 0.01f, 0.0f, 10.0f, "%.3f");
        int iterations = static_cast<int>(params_.iterations);
        if (ImGui::DragInt("Iterations", &iterations, 1.0f, 1, 16)) {
            params_.iterations = static_cast<std::uint32_t>(std::clamp(iterations, 1, 16));
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = JSON::object();
        json["threshold"] = params_.threshold;
        json["softKnee"] = params_.softKnee;
        json["intensity"] = params_.intensity;
        json["blurRadius"] = params_.blurRadius;
        json["iterations"] = params_.iterations;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        params_.threshold = json.value("threshold", 1.0f);
        params_.softKnee = json.value("softKnee", 0.5f);
        params_.intensity = json.value("intensity", 0.8f);
        params_.blurRadius = json.value("blurRadius", 1.0f);
        params_.iterations = std::clamp(json.value("iterations", 4u), 1u, 16u);
        return true;
    }

    /// @brief カスタム描画のみで完結するため宣言的なパスは返さない
    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        static const char *kPrefilter = "PostEffect.Bloom.Prefilter";
        static const char *kDownsample = "PostEffect.Bloom.Downsample";
        static const char *kBlur = "PostEffect.Bloom.Blur";
        static const char *kUpsample = "PostEffect.Bloom.Upsample";
        static const char *kComposite = "PostEffect.Bloom.Composite";
        if (!context.pipelineManager->HasPipeline(kPrefilter) ||
            !context.pipelineManager->HasPipeline(kDownsample) ||
            !context.pipelineManager->HasPipeline(kBlur) ||
            !context.pipelineManager->HasPipeline(kUpsample) ||
            !context.pipelineManager->HasPipeline(kComposite)) {
            return false;
        }

        if (!EnsureIntermediateTargets(screenBuffer)) return false;
        if (!UpdateConstantBuffer()) return false;

        const std::uint32_t levels = std::clamp<std::uint32_t>(params_.iterations, 1u, 16u);

        // シーン色を読み取り可能にし、最終出力用の書き込み面を開く
        screenBuffer->NextPass();

        //--------- 内部レンダーターゲットへの1パス描画ヘルパー ---------//
        auto drawTo = [&](LevelRT &dst, const char *pipelineName, auto &&bindFunc) {
            dst.rt->SetCommandList(commandList);
            if (!dst.rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return;

            const auto rtv = dst.rt->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            dst.rt->ClearRenderTargetView();

            D3D12_VIEWPORT vp{};
            vp.Width = static_cast<float>(dst.width);
            vp.Height = static_cast<float>(dst.height);
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            D3D12_RECT sc{};
            sc.right = static_cast<LONG>(dst.width);
            sc.bottom = static_cast<LONG>(dst.height);
            commandList->RSSetViewports(1, &vp);
            commandList->RSSetScissorRects(1, &sc);

            context.pipelineBinder->UsePipeline(pipelineName);
            auto &binder = context.getShaderBinder(pipelineName);
            binder.Bind("Pixel:BloomCB", constantBuffer_.get());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            bindFunc(binder);

            commandList->DrawInstanced(3, 1, 0, 0);

            dst.rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        };

        // Prefilter: シーン色 -> pyramid[0]
        drawTo(pyramid_[0], kPrefilter, [&](ShaderVariableBinder &binder) {
            binder.Bind("Pixel:gTexture", screenBuffer->GetSrvHandle());
        });
        // Blur: pyramid[0] -> pyramidBlur[0]
        drawTo(pyramidBlur_[0], kBlur, [&](ShaderVariableBinder &binder) {
            binder.Bind("Pixel:gTexture", pyramid_[0].srv->GetGPUDescriptorHandle());
        });

        // Downsample + Blur チェーン
        for (std::uint32_t i = 1; i < levels; ++i) {
            drawTo(pyramid_[i], kDownsample, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:gTexture", pyramidBlur_[i - 1].srv->GetGPUDescriptorHandle());
            });
            drawTo(pyramidBlur_[i], kBlur, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:gTexture", pyramid_[i].srv->GetGPUDescriptorHandle());
            });
        }

        // Upsample 累積（最下層から上へ加算合成）
        const std::uint32_t last = levels - 1;
        drawTo(pyramidAccum_[last], kUpsample, [&](ShaderVariableBinder &binder) {
            binder.Bind("Pixel:gLowTexture", pyramidBlur_[last].srv->GetGPUDescriptorHandle());
            binder.Bind("Pixel:gHighTexture", pyramidBlur_[last].srv->GetGPUDescriptorHandle());
        });
        for (std::uint32_t i = last; i >= 1; --i) {
            const std::uint32_t dst = i - 1;
            drawTo(pyramidAccum_[dst], kUpsample, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:gLowTexture", pyramidAccum_[i].srv->GetGPUDescriptorHandle());
                binder.Bind("Pixel:gHighTexture", pyramidBlur_[dst].srv->GetGPUDescriptorHandle());
            });
        }

        // Composite: シーン色 + ブルームをオーナーの書き込み面へ
        screenBuffer->RebindWriteTarget();
        context.pipelineBinder->UsePipeline(kComposite);
        auto &binder = context.getShaderBinder(kComposite);
        binder.Bind("Pixel:BloomCB", constantBuffer_.get());
        binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
        binder.Bind("Pixel:gBloomTexture", pyramidAccum_[0].srv->GetGPUDescriptorHandle());
        SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
        commandList->DrawInstanced(3, 1, 0, 0);

        return true;
    }

private:
    /// @brief BloomCB 定数バッファと同レイアウトの構造体
    struct CBData {
        float threshold = 1.0f;
        float softKnee = 0.5f;
        float intensity = 0.8f;
        float blurRadius = 1.0f;
    };

    /// @brief 縮小ピラミッドの1レベル分のレンダーターゲット
    struct LevelRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    /// @brief 中間レンダーターゲット群をオーナーのサイズ・フォーマットに合わせて用意する
    bool EnsureIntermediateTargets(ScreenBuffer *owner) {
        const std::uint32_t baseWidth = owner->GetWidth();
        const std::uint32_t baseHeight = owner->GetHeight();
        if (baseWidth == 0 || baseHeight == 0) return false;

        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (auto *rt = owner->GetRenderTarget()) {
            format = rt->GetFormat();
        }

        const std::uint32_t levels = std::clamp<std::uint32_t>(params_.iterations, 1u, 16u);

        if (pyramidFormat_ != format || pyramid_.size() != levels) {
            pyramidFormat_ = format;
            pyramid_.clear();
            pyramid_.resize(levels);
            pyramidBlur_.clear();
            pyramidBlur_.resize(levels);
            pyramidAccum_.clear();
            pyramidAccum_.resize(levels);
        }

        std::uint32_t width = std::max(1u, baseWidth / 2u);
        std::uint32_t height = std::max(1u, baseHeight / 2u);
        for (std::uint32_t i = 0; i < levels; ++i) {
            auto ensureLevel = [&](LevelRT &level) {
                if (!level.rt || !level.srv || level.width != width || level.height != height) {
                    level.rt = std::make_unique<RenderTargetResource>(width, height, format);
                    level.srv = std::make_unique<ShaderResourceResource>(level.rt.get());
                    level.width = width;
                    level.height = height;
                }
            };
            ensureLevel(pyramid_[i]);
            ensureLevel(pyramidBlur_[i]);
            ensureLevel(pyramidAccum_[i]);

            width = std::max(1u, width / 2u);
            height = std::max(1u, height / 2u);
        }

        return pyramid_[0].rt && pyramid_[0].srv;
    }

    /// @brief 定数バッファへ現在のパラメータをアップロードする
    bool UpdateConstantBuffer() {
        if (!constantBuffer_) {
            constantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CBData));
        }
        void *mapped = constantBuffer_->Map();
        if (!mapped) return false;
        CBData data;
        data.threshold = params_.threshold;
        data.softKnee = params_.softKnee;
        data.intensity = params_.intensity;
        data.blurRadius = params_.blurRadius;
        std::memcpy(mapped, &data, sizeof(data));
        return true;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> constantBuffer_;

    std::vector<LevelRT> pyramid_;
    std::vector<LevelRT> pyramidBlur_;
    std::vector<LevelRT> pyramidAccum_;
    DXGI_FORMAT pyramidFormat_ = DXGI_FORMAT_UNKNOWN;
};

REGISTER_COMPONENT_OBJECT(BloomEffect)

} // namespace KashipanEngine
