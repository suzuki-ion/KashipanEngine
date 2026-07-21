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

namespace KashipanEngine {

/// @brief アンビエントオクルージョン（SSAO）ポストエフェクト
/// @details 専用の法線バッファ（G-buffer）を持たないフォワードレンダリングのため、
///          深度バッファのみからスクリーンスペースでワールド座標・法線を再構成する簡易SSAOとして実装する。
///          既にシェーディング済みのシーン色へAOを乗算するため、間接光だけでなく鏡面成分にも
///          多少影響してしまう点はこの方式の既知の妥協点。
///          中間レンダーターゲット（AO計算結果・ブラー結果）を使う多段パスのため、
///          宣言的なパス定義ではなくカスタム描画で実装している（BloomEffectと同様の構成）。
class AmbientOcclusionEffect final : public IPostProcessComponent {
public:
    struct Params {
        float radius = 0.5f;       // 遮蔽を判定するワールド空間の半径
        float intensity = 1.0f;    // 遮蔽の強さ
        float power = 1.0f;        // AO値のコントラスト（大きいほど暗部が締まる）
        float bias = 0.02f;        // 自己遮蔽を避けるためのワールド距離バイアス
        std::uint32_t sampleCount = 16; // 半球サンプル数 (4..64)
        int blurRadius = 2;        // ブラーのサンプル半径（ピクセル）
        float depthThreshold = 0.5f; // ブラー時、これを超える線形深度差のサンプルは除外する
    };

    AmbientOcclusionEffect() : IPostProcessComponent("AmbientOcclusionEffect") {
        ADD_MEMBER_VARIABLE(params_.radius);
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.power);
        ADD_MEMBER_VARIABLE(params_.bias);
        ADD_MEMBER_VARIABLE(params_.sampleCount);
        ADD_MEMBER_VARIABLE(params_.blurRadius);
        ADD_MEMBER_VARIABLE(params_.depthThreshold);
    }
    ~AmbientOcclusionEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<AmbientOcclusionEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        aoRaw_ = {};
        aoBlurred_ = {};
        aoConstantBuffer_.reset();
        blurConstantBuffer_.reset();
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Radius", &params_.radius, 0.01f, 0.01f, 20.0f, "%.3f");
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGui::DragFloat("Power", &params_.power, 0.01f, 0.1f, 8.0f, "%.3f");
        ImGui::DragFloat("Bias", &params_.bias, 0.001f, 0.0f, 1.0f, "%.4f");
        int sampleCount = static_cast<int>(params_.sampleCount);
        if (ImGui::DragInt("Sample Count", &sampleCount, 1.0f, 4, 64)) {
            params_.sampleCount = static_cast<std::uint32_t>(std::clamp(sampleCount, 4, 64));
        }
        ImGui::DragInt("Blur Radius", &params_.blurRadius, 1.0f, 0, 8);
        ImGui::DragFloat("Depth Threshold", &params_.depthThreshold, 0.01f, 0.001f, 100.0f, "%.3f");
        if (!lastCameraValid_) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                "No camera resolved for this ScreenBuffer yet: AO will not be applied.");
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["radius"] = params_.radius;
        json["intensity"] = params_.intensity;
        json["power"] = params_.power;
        json["bias"] = params_.bias;
        json["sampleCount"] = params_.sampleCount;
        json["blurRadius"] = params_.blurRadius;
        json["depthThreshold"] = params_.depthThreshold;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.radius = json.value("radius", 0.5f);
        params_.intensity = json.value("intensity", 1.0f);
        params_.power = json.value("power", 1.0f);
        params_.bias = json.value("bias", 0.02f);
        params_.sampleCount = std::clamp(json.value("sampleCount", 16u), 4u, 64u);
        params_.blurRadius = std::clamp(json.value("blurRadius", 2), 0, 8);
        params_.depthThreshold = json.value("depthThreshold", 0.5f);
        return true;
    }

    /// @brief カスタム描画のみで完結するため宣言的なパスは返さない
    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        const CameraInfo &cameraInfo = GetCameraInfo();
        lastCameraValid_ = cameraInfo.valid;
        // カメラが解決できない場合はワールド座標の再構成ができないため、何もせず元の描画結果を残す
        if (!cameraInfo.valid) return false;

        static const char *kAO = "PostEffect.AmbientOcclusion";
        static const char *kBlur = "PostEffect.AmbientOcclusionBlur";
        static const char *kComposite = "PostEffect.AmbientOcclusionComposite";
        if (!context.pipelineManager->HasPipeline(kAO) ||
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

        // AO計算パス: 深度のみからワールド座標・法線を再構成して遮蔽率を求める
        {
            AOParamsCBData cbData{};
            cbData.viewProjection = cameraInfo.viewProjection;
            cbData.invViewProjection = cameraInfo.viewProjection.Inverse();
            cbData.cameraWorldPosition = cameraInfo.worldPosition;
            cbData.radius = params_.radius;
            cbData.intensity = params_.intensity;
            cbData.power = params_.power;
            cbData.bias = params_.bias;
            cbData.sampleCount = params_.sampleCount;
            cbData.texelSize[0] = (aoRaw_.width > 0) ? (1.0f / static_cast<float>(aoRaw_.width)) : 0.0f;
            cbData.texelSize[1] = (aoRaw_.height > 0) ? (1.0f / static_cast<float>(aoRaw_.height)) : 0.0f;
            cbData.rotationSeed = 0.0f;
            if (!aoConstantBuffer_) aoConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(AOParamsCBData));
            if (void *mapped = aoConstantBuffer_->Map()) {
                std::memcpy(mapped, &cbData, sizeof(cbData));
            }

            drawTo(aoRaw_, kAO, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:AOParamsCB", aoConstantBuffer_.get());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        }

        // ブラーパス: 深度差を考慮してAOのノイズを除去する
        {
            AOBlurCBData cbData{};
            cbData.texelSize[0] = (aoRaw_.width > 0) ? (1.0f / static_cast<float>(aoRaw_.width)) : 0.0f;
            cbData.texelSize[1] = (aoRaw_.height > 0) ? (1.0f / static_cast<float>(aoRaw_.height)) : 0.0f;
            cbData.radius = params_.blurRadius;
            cbData.depthThreshold = params_.depthThreshold;
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            if (!blurConstantBuffer_) blurConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(AOBlurCBData));
            if (void *mapped = blurConstantBuffer_->Map()) {
                std::memcpy(mapped, &cbData, sizeof(cbData));
            }

            drawTo(aoBlurred_, kBlur, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:AOBlurCB", blurConstantBuffer_.get());
                binder.Bind("Pixel:gTexture", aoRaw_.srv->GetGPUDescriptorHandle());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        }

        // 合成パス: シーン色へAOを乗算し、オーナーの書き込み面へ出力する
        screenBuffer->RebindWriteTarget();
        context.pipelineBinder->UsePipeline(kComposite);
        auto &binder = context.getShaderBinder(kComposite);
        binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
        binder.Bind("Pixel:gAOTexture", aoBlurred_.srv->GetGPUDescriptorHandle());
        SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
        commandList->DrawInstanced(3, 1, 0, 0);

        return true;
    }

private:
    /// @brief AOParamsCB（AmbientOcclusionCB.hlsli）と同レイアウトの構造体
    struct AOParamsCBData {
        Matrix4x4 viewProjection = Matrix4x4::Identity();
        Matrix4x4 invViewProjection = Matrix4x4::Identity();
        Vector3 cameraWorldPosition{ 0.0f, 0.0f, 0.0f };
        float radius = 0.5f;
        float intensity = 1.0f;
        float power = 1.0f;
        float bias = 0.02f;
        std::uint32_t sampleCount = 16;
        float texelSize[2]{};
        float rotationSeed = 0.0f;
        float pad = 0.0f;
    };
    /// @brief AOBlurCB（AmbientOcclusionBlurCB.hlsli）と同レイアウトの構造体
    struct AOBlurCBData {
        float texelSize[2]{};
        int radius = 2;
        float depthThreshold = 0.5f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
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
        ensure(aoRaw_);
        ensure(aoBlurred_);

        return aoRaw_.rt && aoRaw_.srv && aoBlurred_.rt && aoBlurred_.srv;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> aoConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> blurConstantBuffer_;
    EffectRT aoRaw_;
    EffectRT aoBlurred_;
    /// @brief 直近のRenderCustomでカメラが解決できたか（ImGuiでの状態表示用）
    bool lastCameraValid_ = true;
};

REGISTER_COMPONENT_OBJECT(AmbientOcclusionEffect)

} // namespace KashipanEngine
