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

/// @brief カメラ動作由来のモーションブラー・ポストエフェクト
/// @details オブジェクトごとの速度G-bufferは持たず、深度から再構成したワールド座標を
///          前フレームのView-Projectionへ再投影してスクリーンスペース速度を求める簡易実装のため、
///          カメラの移動・回転によるブラーのみが対象（オブジェクト自身の動きには追従しない）。
///          中間レンダーターゲット（速度バッファ）を使う多段パスのため、
///          宣言的なパス定義ではなくカスタム描画で実装している（AmbientOcclusionEffectと同様の構成）。
class MotionBlurEffect final : public IPostProcessComponent {
public:
    struct Params {
        float intensity = 1.0f;        // ブラー全体の強さ
        float velocityScale = 1.0f;    // 速度からブラー量への倍率
        float maxBlurPixels = 32.0f;   // ブラー量の上限（ピクセル）
        std::uint32_t samples = 8;     // ブラーのサンプル数
    };

    MotionBlurEffect() : IPostProcessComponent("MotionBlurEffect", GetComponentTypeID<MotionBlurEffect>()) {
        ADD_MEMBER_VARIABLE(params_.intensity);
        ADD_MEMBER_VARIABLE(params_.velocityScale);
        ADD_MEMBER_VARIABLE(params_.maxBlurPixels);
        ADD_MEMBER_VARIABLE(params_.samples);
    }
    ~MotionBlurEffect() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto ptr = std::make_unique<MotionBlurEffect>();
        ptr->params_ = params_;
        return ptr;
    }

    void SetParams(const Params &params) { params_ = params; }
    const Params &GetParams() const noexcept { return params_; }

protected:
    void Finalize() override {
        IPostProcessComponent::Finalize();
        velocity_ = {};
        velocityConstantBuffer_.reset();
        blurConstantBuffer_.reset();
        hasPreviousViewProjection_ = false;
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        IPostProcessComponent::ShowImGui();
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ブラー全体の強さ");
        }
        ImGui::DragFloat("Velocity Scale", &params_.velocityScale, 0.01f, 0.0f, 10.0f, "%.3f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("速度からブラー量への倍率");
        }
        ImGui::DragFloat("Max Blur Pixels", &params_.maxBlurPixels, 0.5f, 0.0f, 256.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ブラー量の上限（ピクセル）");
        }
        int samples = static_cast<int>(params_.samples);
        if (ImGui::DragInt("Samples", &samples, 1.0f, 2, 32)) {
            params_.samples = static_cast<std::uint32_t>(std::clamp(samples, 2, 32));
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("ブラーのサンプル数");
        }
        if (!GetCameraInfo().valid) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                "No camera resolved for this ScreenBuffer yet: Motion Blur will not be applied.");
        }
    }
#endif

    JSON SaveToJson() const override {
        JSON json = IPostProcessComponent::SaveToJson();
        json["intensity"] = params_.intensity;
        json["velocityScale"] = params_.velocityScale;
        json["maxBlurPixels"] = params_.maxBlurPixels;
        json["samples"] = params_.samples;
        return json;
    }

    bool LoadFromJson(const JSON &json) override {
        IPostProcessComponent::LoadFromJson(json);
        params_.intensity = json.value("intensity", 1.0f);
        params_.velocityScale = json.value("velocityScale", 1.0f);
        params_.maxBlurPixels = json.value("maxBlurPixels", 32.0f);
        params_.samples = std::clamp(json.value("samples", 8u), 2u, 32u);
        return true;
    }

    /// @brief カスタム描画のみで完結するため宣言的なパスは返さない
    std::vector<PassInfo> BuildPasses() override { return {}; }

    bool RenderCustom(CustomRenderContext &context) override {
        auto *screenBuffer = context.screenBuffer;
        auto *commandList = context.commandList;
        if (!screenBuffer || !commandList || !context.pipelineManager || !context.pipelineBinder || !context.getShaderBinder) return false;

        const CameraInfo &cameraInfo = GetCameraInfo();
        // カメラが解決できない場合は前フレームとの再投影ができないため、何もせず元の描画結果を残す
        if (!cameraInfo.valid) return false;

        static const char *kVelocity = "PostEffect.MotionBlur.Velocity";
        static const char *kBlur = "PostEffect.MotionBlur";
        if (!context.pipelineManager->HasPipeline(kVelocity) ||
            !context.pipelineManager->HasPipeline(kBlur)) {
            return false;
        }

        if (!EnsureIntermediateTargets(screenBuffer)) return false;

        // 前フレームのView-Projectionが無い（初回・リセット直後）場合は今フレームの値で代用し、速度0扱いにする
        const Matrix4x4 prevViewProjection = hasPreviousViewProjection_ ? previousViewProjection_ : cameraInfo.viewProjection;

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

        // 速度パス: 深度から再構成したワールド座標を前フレームのView-Projectionへ再投影し、
        // 画面上の見かけ上の移動量（カメラ動作分のみ）を求める
        {
            MotionBlurVelocityCBData cbData{};
            cbData.invViewProjection = cameraInfo.viewProjection.Inverse();
            cbData.prevViewProjection = prevViewProjection;
            cbData.nearClip = cameraInfo.nearClip;
            cbData.farClip = cameraInfo.farClip;
            if (!velocityConstantBuffer_) velocityConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(MotionBlurVelocityCBData));
            if (void *mapped = velocityConstantBuffer_->Map()) {
                std::memcpy(mapped, &cbData, sizeof(cbData));
            }

            drawTo(velocity_, kVelocity, [&](ShaderVariableBinder &binder) {
                binder.Bind("Pixel:MotionBlurVelocityCB", velocityConstantBuffer_.get());
                binder.Bind("Pixel:gDepthTexture", screenBuffer->GetDepthSrvHandle());
                SamplerManager::BindSampler(&binder, "Pixel:gDepthSampler", DefaultSampler::PointClamp);
            });
        }

        // ブラーパス: 速度方向へシーン色をガザリングブラーし、オーナーの書き込み面へ出力する
        screenBuffer->RebindWriteTarget();
        {
            MotionBlurCBData cbData{};
            cbData.intensity = params_.intensity;
            cbData.velocityScale = params_.velocityScale;
            cbData.maxBlurPixels = params_.maxBlurPixels;
            cbData.samples = params_.samples;
            cbData.invResolution[0] = (velocity_.width > 0) ? (1.0f / static_cast<float>(velocity_.width)) : 0.0f;
            cbData.invResolution[1] = (velocity_.height > 0) ? (1.0f / static_cast<float>(velocity_.height)) : 0.0f;
            if (!blurConstantBuffer_) blurConstantBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(MotionBlurCBData));
            if (void *mapped = blurConstantBuffer_->Map()) {
                std::memcpy(mapped, &cbData, sizeof(cbData));
            }

            context.pipelineBinder->UsePipeline(kBlur);
            auto &binder = context.getShaderBinder(kBlur);
            binder.Bind("Pixel:MotionBlurCB", blurConstantBuffer_.get());
            binder.Bind("Pixel:gSceneTexture", screenBuffer->GetSrvHandle());
            binder.Bind("Pixel:gVelocityTexture", velocity_.srv->GetGPUDescriptorHandle());
            SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp);
            commandList->DrawInstanced(3, 1, 0, 0);
        }

        previousViewProjection_ = cameraInfo.viewProjection;
        hasPreviousViewProjection_ = true;

        return true;
    }

private:
    /// @brief MotionBlurVelocityCB（MotionBlurVelocityCB.hlsli）と同レイアウトの構造体
    struct MotionBlurVelocityCBData {
        Matrix4x4 invViewProjection = Matrix4x4::Identity();
        Matrix4x4 prevViewProjection = Matrix4x4::Identity();
        float nearClip = 0.1f;
        float farClip = 1000.0f;
        float pad[2]{};
    };
    /// @brief MotionBlurCB（MotionBlurCB.hlsli）と同レイアウトの構造体
    struct MotionBlurCBData {
        float intensity = 1.0f;
        float velocityScale = 1.0f;
        float maxBlurPixels = 32.0f;
        std::uint32_t samples = 8;
        float invResolution[2]{};
        float pad[2]{};
    };

    /// @brief カスタムエフェクトが使う単一レベルの中間レンダーターゲット
    struct EffectRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    /// @brief 中間レンダーターゲット群をオーナーのサイズに合わせて用意する
    bool EnsureIntermediateTargets(ScreenBuffer *owner) {
        const std::uint32_t width = owner->GetWidth();
        const std::uint32_t height = owner->GetHeight();
        if (width == 0 || height == 0) return false;

        // 速度バッファは符号付きのUV空間速度をそのまま保持するため、8bit UNORM（オーナーの色フォーマット）
        // ではなく16bit浮動小数点フォーマットを使う。UNORMだと1LSBの量子化幅がスクリーン解像度換算で
        // 数ピクセル分にもなり、Velocity Scale/Intensityを上げた際に量子化誤差がそのまま
        // 不自然なブラーとして目立ってしまうため
        constexpr DXGI_FORMAT kVelocityFormat = DXGI_FORMAT_R16G16_FLOAT;

        if (!velocity_.rt || !velocity_.srv || velocity_.width != width || velocity_.height != height) {
            velocity_.rt = std::make_unique<RenderTargetResource>(width, height, kVelocityFormat);
            velocity_.srv = std::make_unique<ShaderResourceResource>(velocity_.rt.get());
            velocity_.width = width;
            velocity_.height = height;
        }

        return velocity_.rt && velocity_.srv;
    }

    Params params_{};
    std::unique_ptr<ConstantBufferResource> velocityConstantBuffer_;
    std::unique_ptr<ConstantBufferResource> blurConstantBuffer_;
    EffectRT velocity_;
    /// @brief 前フレームのカメラView-Projection（再投影による速度計算に使う）
    Matrix4x4 previousViewProjection_ = Matrix4x4::Identity();
    bool hasPreviousViewProjection_ = false;
};

REGISTER_COMPONENT_OBJECT(MotionBlurEffect)

} // namespace KashipanEngine
