#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <d3d12.h>
#include <memory>
#include <string>

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

/// @brief 被写界深度（DoF）ポストエフェクト
/// @details 深度とフォーカス距離からCoC（錯乱円）を近景・遠景に分けて求め、それぞれを
///          別々にガザリングブラーしてから合成する（前景と背景を分離しない単一パス方式では、
///          手前のシャープな物体の色が奥のボケた背景ににじむ問題があるため、この方式を採る）。
///          近景CoCはダイレーション（最大値フィルタ）で膨張させ、前景のボケがその物体本来の
///          シルエットより外側にも自然に広がるようにしている。
///          中間レンダーターゲットを使う多段パスのため、宣言的なパス定義ではなく
///          カスタム描画で実装している（BloomEffect/AmbientOcclusionEffectと同様の構成）。
class DepthOfFieldEffect final : public IPostProcessComponent {
public:
    struct Params {
        float focusDistance = 10.0f;     // ピントを合わせるワールド距離
        float focusRange = 2.0f;         // ピントが合っているとみなす幅（この範囲内はボケない）
        float nearBlurDistance = 5.0f;   // 手前側、ここまでの距離で近景ボケが最大になる
        float farBlurDistance = 10.0f;   // 奥側、ここまでの距離で遠景ボケが最大になる
        float maxBlurRadiusPixels = 16.0f; // 最大ぼかし半径（ピクセル）
        std::uint32_t sampleCount = 16;  // ブラーのサンプル数
        int dilateRadius = 4;            // 近景CoCダイレーションのサンプル半径（ピクセル）
    };

    DepthOfFieldEffect() : IPostProcessComponent("DepthOfFieldEffect", GetComponentTypeID<DepthOfFieldEffect>()) {
        ADD_MEMBER_VARIABLE(params_.focusDistance);
        ADD_MEMBER_VARIABLE(params_.focusRange);
        ADD_MEMBER_VARIABLE(params_.nearBlurDistance);
        ADD_MEMBER_VARIABLE(params_.farBlurDistance);
        ADD_MEMBER_VARIABLE(params_.maxBlurRadiusPixels);
        ADD_MEMBER_VARIABLE(params_.sampleCount);
        ADD_MEMBER_VARIABLE(params_.dilateRadius);
    }
    ~DepthOfFieldEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<DepthOfFieldEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        IPostProcessComponent::Finalize();
        coc_ = {};
        dilatedNear_ = {};
        farBlurred_ = {};
        nearBlurred_ = {};
        cocConstantBuffer_.reset();
        dilateConstantBuffer_.reset();
        farBlurConstantBuffer_.reset();
        nearBlurConstantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat(TranslationLabel("component.depthoffieldeffect.focus_distance"), &params_.focusDistance, 0.1f, 0.0f, 10000.0f, "%.2f");
        ImGui::DragFloat(TranslationLabel("component.depthoffieldeffect.focus_range"), &params_.focusRange, 0.05f, 0.0f, 1000.0f, "%.2f");
        ImGui::DragFloat(TranslationLabel("component.depthoffieldeffect.near_blur_distance"), &params_.nearBlurDistance, 0.1f, 0.01f, 1000.0f, "%.2f");
        ImGui::DragFloat(TranslationLabel("component.depthoffieldeffect.far_blur_distance"), &params_.farBlurDistance, 0.1f, 0.01f, 1000.0f, "%.2f");
        ImGui::DragFloat(TranslationLabel("component.depthoffieldeffect.max_blur_radius_px"), &params_.maxBlurRadiusPixels, 0.5f, 0.0f, 128.0f, "%.1f");
        int sampleCount = static_cast<int>(params_.sampleCount);
        if (ImGui::DragInt(TranslationLabel("component.depthoffieldeffect.sample_count"), &sampleCount, 1.0f, 4, 64)) {
            params_.sampleCount = static_cast<std::uint32_t>(std::clamp(sampleCount, 4, 64));
        }
        ImGui::DragInt(TranslationLabel("component.depthoffieldeffect.dilate_radius_px"), &params_.dilateRadius, 1.0f, 0, 16);
        if (!GetCameraInfo().valid) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                "No camera resolved for this ScreenBuffer yet: DoF will not be applied.");
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["focusDistance"] = params_.focusDistance;
        json["focusRange"] = params_.focusRange;
        json["nearBlurDistance"] = params_.nearBlurDistance;
        json["farBlurDistance"] = params_.farBlurDistance;
        json["maxBlurRadiusPixels"] = params_.maxBlurRadiusPixels;
        json["sampleCount"] = params_.sampleCount;
        json["dilateRadius"] = params_.dilateRadius;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.focusDistance = json.value("focusDistance", 10.0f);
        params_.focusRange = json.value("focusRange", 2.0f);
        params_.nearBlurDistance = json.value("nearBlurDistance", 5.0f);
        params_.farBlurDistance = json.value("farBlurDistance", 10.0f);
        params_.maxBlurRadiusPixels = json.value("maxBlurRadiusPixels", 16.0f);
        params_.sampleCount = std::clamp(json.value("sampleCount", 16u), 4u, 64u);
        params_.dilateRadius = std::clamp(json.value("dilateRadius", 4), 0, 16);
        return true;
    }

    /// @brief カスタム描画のみで完結するため宣言的なパスは返さない
    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        const CameraInfo &cameraInfo = GetCameraInfo();
        // カメラが解決できない場合はNear/Farの線形化ができないため、何もせず元の描画結果を残す
        if (!cameraInfo.valid) return false;

        static const char *kCoC = "PostEffect.DepthOfField.CoC";
        static const char *kDilate = "PostEffect.DepthOfField.Dilate";
        static const char *kBlur = "PostEffect.DepthOfField.Blur";
        static const char *kComposite = "PostEffect.DepthOfField.Composite";
        if (!context.pipelineManager->HasPipeline(kCoC) ||
            !context.pipelineManager->HasPipeline(kDilate) ||
            !context.pipelineManager->HasPipeline(kBlur) ||
            !context.pipelineManager->HasPipeline(kComposite)) {
            return false;
        }

        if (!EnsureIntermediateTargets(screenBuffer)) return false;

        // シーン色を読み取り可能にし、最終出力用の書き込み面を開く
        screenBuffer->NextPass();

        //--------- 内部レンダーターゲットへの1パス描画ヘルパー ---------//
        auto drawTo = [&](EffectRT &dst, const char *pipelineName, auto &&bindFunc) {
            dst.rt->SetCommandList(commandList);
            if (!dst.rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return;

            const auto rtv = dst.rt->GetCPUDescriptorHandle();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

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
            bindFunc(binder);

            commandList->DrawInstanced(3, 1, 0, 0);

            dst.rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        };

        const float texelX = (coc_.width > 0) ? (1.0f / static_cast<float>(coc_.width)) : 0.0f;
        const float texelY = (coc_.height > 0) ? (1.0f / static_cast<float>(coc_.height)) : 0.0f;

        // CoC計算パス: 深度とフォーカス距離から近景・遠景のCoCを求める
        {
            DoFCoCCBData cbData{};
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            cbData.focusDistance = params_.focusDistance;
            cbData.focusRange = params_.focusRange;
            cbData.nearBlurDistance = params_.nearBlurDistance;
            cbData.farBlurDistance = params_.farBlurDistance;
            if (!cocConstantBuffer_) cocConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DoFCoCCBData));
            if (void *mapped = cocConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(coc_, kCoC, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:DoFCoCCB", cocConstantBuffer_.get());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        }

        // ダイレーションパス: 近景CoCを周辺の最大値で膨張させ、前景ボケがシルエット外側にも広がるようにする
        {
            DoFDilateCBData cbData{};
            cbData.texelSize[0] = texelX;
            cbData.texelSize[1] = texelY;
            cbData.radius = params_.dilateRadius;
            if (!dilateConstantBuffer_) dilateConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DoFDilateCBData));
            if (void *mapped = dilateConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(dilatedNear_, kDilate, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:DoFDilateCB", dilateConstantBuffer_.get());
                binder.Bind("Pixel:gCoCTexture", coc_.srv->GetGPUDescriptorHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::PointClamp);
            });
        }

        // 遠景ブラーパス: 遠景CoC（coc_のrチャンネル）を半径としてシャープなシーン色をガザリングブラーする
        {
            DoFBlurCBData cbData{};
            cbData.texelSize[0] = texelX;
            cbData.texelSize[1] = texelY;
            cbData.maxBlurRadiusUv[0] = params_.maxBlurRadiusPixels * texelX;
            cbData.maxBlurRadiusUv[1] = params_.maxBlurRadiusPixels * texelY;
            cbData.sampleCount = params_.sampleCount;
            cbData.rotationSeed = 0.0f;
            if (!farBlurConstantBuffer_) farBlurConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DoFBlurCBData));
            if (void *mapped = farBlurConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(farBlurred_, kBlur, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:DoFBlurCB", farBlurConstantBuffer_.get());
                binder.Bind("Pixel:gTexture", screenBuffer->GetSrvHandle());
                binder.Bind("Pixel:gCoCTexture", coc_.srv->GetGPUDescriptorHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
                SamplerManager::BindSampler(&binder, "Pixel:gCoCSampler", DefaultSampler::PointClamp);
            });
        }

        // 近景ブラーパス: ダイレーション後の近景CoCを半径として同様にガザリングブラーする
        {
            DoFBlurCBData cbData{};
            cbData.texelSize[0] = texelX;
            cbData.texelSize[1] = texelY;
            cbData.maxBlurRadiusUv[0] = params_.maxBlurRadiusPixels * texelX;
            cbData.maxBlurRadiusUv[1] = params_.maxBlurRadiusPixels * texelY;
            cbData.sampleCount = params_.sampleCount;
            cbData.rotationSeed = 1.5707963f; // 遠景パスと位相をずらしてノイズパターンを変える
            if (!nearBlurConstantBuffer_) nearBlurConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DoFBlurCBData));
            if (void *mapped = nearBlurConstantBuffer_->Map()) std::memcpy(mapped, &cbData, sizeof(cbData));

            drawTo(nearBlurred_, kBlur, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:DoFBlurCB", nearBlurConstantBuffer_.get());
                binder.Bind("Pixel:gTexture", screenBuffer->GetSrvHandle());
                binder.Bind("Pixel:gCoCTexture", dilatedNear_.srv->GetGPUDescriptorHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
                SamplerManager::BindSampler(&binder, "Pixel:gCoCSampler", DefaultSampler::PointClamp);
            });
        }

        // 合成パス: シャープな画像へ遠景ブラー→近景ブラーの順で合成し、オーナーの書き込み面へ出力する
        screenBuffer->RebindWriteTarget();
        context.pipelineBinder->UsePipeline(kComposite);
        auto &binder = context.getShaderBinder(kComposite);
        binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
        binder.Bind("Pixel:gFarBlurTexture", farBlurred_.srv->GetGPUDescriptorHandle());
        binder.Bind("Pixel:gNearBlurTexture", nearBlurred_.srv->GetGPUDescriptorHandle());
        binder.Bind("Pixel:gCoCTexture", coc_.srv->GetGPUDescriptorHandle());
        binder.Bind("Pixel:gNearMaskTexture", dilatedNear_.srv->GetGPUDescriptorHandle());
        SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
        commandList->DrawInstanced(3, 1, 0, 0);

        return true;
    }

private:
    /// @brief DoFCoCCB（DepthOfFieldCoCCB.hlsli）と同レイアウトの構造体
    struct DoFCoCCBData {
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        float focusDistance = 10.0f;
        float focusRange = 2.0f;
        float nearBlurDistance = 5.0f;
        float farBlurDistance = 10.0f;
        float pad[2]{};
    };
    /// @brief DoFDilateCB（DepthOfFieldDilateCB.hlsli）と同レイアウトの構造体
    struct DoFDilateCBData {
        float texelSize[2]{};
        int radius = 4;
        float pad = 0.0f;
    };
    /// @brief DoFBlurCB（DepthOfFieldBlurCB.hlsli）と同レイアウトの構造体
    struct DoFBlurCBData {
        float texelSize[2]{};
        float maxBlurRadiusUv[2]{};
        std::uint32_t sampleCount = 16;
        float rotationSeed = 0.0f;
        float pad[2]{};
    };

    /// @brief カスタムエフェクトが使う単一レベルの中間レンダーターゲット
    struct EffectRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    /// @brief 中間レンダーターゲット群をオーナーのサイズ・フォーマットに合わせて用意する
    bool EnsureIntermediateTargets(ScreenBuffer *owner) {
        const std::uint32_t width = owner->GetWidth();
        const std::uint32_t height = owner->GetHeight();
        if (width == 0 || height == 0) return false;

        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (auto *rt = owner->GetRenderTarget()) {
            format = rt->GetFormat();
        }

        auto ensure = [&](EffectRT &target) {
            if (!target.rt || !target.srv || target.width != width || target.height != height) {
                target.rt = std::make_unique<RenderTargetResource>(width, height, format);
                target.srv = std::make_unique<ShaderResourceResource>(target.rt.get());
                target.width = width;
                target.height = height;
            }
        };
        ensure(coc_);
        ensure(dilatedNear_);
        ensure(farBlurred_);
        ensure(nearBlurred_);

        return coc_.rt && coc_.srv && dilatedNear_.rt && dilatedNear_.srv && farBlurred_.rt && farBlurred_.srv && nearBlurred_.rt && nearBlurred_.srv;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> cocConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> dilateConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> farBlurConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> nearBlurConstantBuffer_;
    EffectRT coc_;
    EffectRT dilatedNear_;
    EffectRT farBlurred_;
    EffectRT nearBlurred_;
};

REGISTER_COMPONENT_OBJECT(DepthOfFieldEffect)

} // namespace KashipanEngine
