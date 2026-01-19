#pragma once
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>

#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Graphics/ScreenBuffer.h"
#include "Assets/SamplerManager.h"
#include "Graphics/Resources/RenderTargetResource.h"
#include "Graphics/Resources/ShaderResourceResource.h"

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

class MotionBlurEffect final : public IPostEffectComponent {
public:
    struct Params {
        float intensity = 1.0f;       // モーションブラー強度
        float velocityScale = 1.0f;   // 擬似 velocity スケール（差分→速度へ）
        float maxBlurPixels = 24.0f;  // 最大ブラー半径（ピクセル）
        std::uint32_t samples = 8;    // サンプル数 (1..32)
    };

    explicit MotionBlurEffect(Params p = {})
        : IPostEffectComponent("MotionBlurEffect", 1), params_(p) {}

    std::optional<bool> Initialize() override {
        return EnsureResources();
    }

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<MotionBlurEffect>(params_);
    }

    void SetVelocityScreenBuffer(ScreenBuffer* buf) { velocityBuffer_ = buf; }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGui::DragFloat("VelocityScale", &params_.velocityScale, 0.01f, 0.0f, 10.0f, "%.3f");
        ImGui::DragFloat("MaxBlurPixels", &params_.maxBlurPixels, 0.1f, 0.0f, 128.0f, "%.1f");
        int s = static_cast<int>(params_.samples);
        if (ImGui::DragInt("Samples", &s, 1.0f, 1, 32)) {
            s = std::clamp(s, 1, 32);
            params_.samples = static_cast<std::uint32_t>(s);
        }
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        auto *owner = GetOwnerBuffer();
        if (!owner) return {};
        if (!EnsureResources()) return {};

        std::vector<PostEffectPass> passes;
        passes.reserve(3);

        auto makeBeginForRT = [](RenderTargetResource *rt, std::uint32_t w, std::uint32_t h, bool clear) {
            return [rt, w, h, clear](ID3D12GraphicsCommandList *cl) -> bool {
                if (!cl) return false;
                if (!rt) return false;

                rt->SetCommandList(cl);
                if (!rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return false;

                const auto rtv = rt->GetCPUDescriptorHandle();
                cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
                if (clear) {
                    rt->ClearRenderTargetView();
                }

                D3D12_VIEWPORT vp{};
                vp.TopLeftX = 0.0f;
                vp.TopLeftY = 0.0f;
                vp.Width = static_cast<float>(w);
                vp.Height = static_cast<float>(h);
                vp.MinDepth = 0.0f;
                vp.MaxDepth = 1.0f;

                D3D12_RECT sc{};
                sc.left = 0;
                sc.top = 0;
                sc.right = static_cast<LONG>(w);
                sc.bottom = static_cast<LONG>(h);

                cl->RSSetViewports(1, &vp);
                cl->RSSetScissorRects(1, &sc);
                return true;
            };
        };

        auto makeEndForRT = [](RenderTargetResource *rt) {
            return [rt](ID3D12GraphicsCommandList *cl) -> bool {
                if (!cl) return false;
                if (!rt) return false;
                rt->SetCommandList(cl);
                return rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            };
        };

        // Pass1: transition prev-frame RT to SRV, render motion blur into internal blurred RT
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.MotionBlur";
            pass.passName = "MotionBlur.ToInternal";
            pass.batchKey = 0;

            pass.beginRecordFunction = [this, makeBeginForRT](ID3D12GraphicsCommandList *cl) -> bool {
                if (!cl) return false;
                if (!historyRt_ || !blurRt_) return false;

                historyRt_->SetCommandList(cl);
                if (!historyRt_->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) return false;

                auto begin = makeBeginForRT(blurRt_.get(), rtW_, rtH_, false);
                return begin(cl);
            };
            pass.endRecordFunction = makeEndForRT(blurRt_.get());

            pass.constantBufferRequirements = {{"Pixel:MotionBlurCB", sizeof(CBData)}};
            pass.updateConstantBuffersFunction = [this](void *constantBufferMaps, std::uint32_t) -> bool {
                if (!constantBufferMaps) return false;
                void **maps = static_cast<void **>(constantBufferMaps);
                auto *cb = reinterpret_cast<CBData *>(maps[0]);
                if (!cb) return false;

                cb->intensity = params_.intensity;
                cb->velocityScale = params_.velocityScale;
                cb->maxBlurPixels = params_.maxBlurPixels;
                cb->samples = static_cast<std::uint32_t>(std::clamp<std::uint32_t>(params_.samples, 1u, 32u));
                cb->invResolution[0] = (rtW_ > 0) ? (1.0f / static_cast<float>(rtW_)) : 0.0f;
                cb->invResolution[1] = (rtH_ > 0) ? (1.0f / static_cast<float>(rtH_)) : 0.0f;
                cb->pad[0] = 0.0f;
                cb->pad[1] = 0.0f;
                return true;
            };

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                auto *o = GetOwnerBuffer();
                if (!o) return false;

                //const auto vel = o->GetVelocitySrvHandle();
                //if (vel.ptr == 0) return false;

                if (!binder.Bind("Pixel:gSceneTexture", o->GetSrvHandle())) return false;
                D3D12_GPU_DESCRIPTOR_HANDLE velHandle{};
                if (velocityBuffer_) velHandle = velocityBuffer_->GetSrvHandle();
                else velHandle = o->GetSrvHandle();
                if (velHandle.ptr == 0) return false;
                if (!binder.Bind("Pixel:gVelocityTexture", velHandle)) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        // Pass2: copy current scene (owner) to prev-frame RT
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.MotionBlur.HistoryCopy";
            pass.passName = "MotionBlur.CopyToPrev";
            pass.batchKey = 0;

            pass.beginRecordFunction = makeBeginForRT(historyRt_.get(), rtW_, rtH_, false);
            pass.endRecordFunction = makeEndForRT(historyRt_.get());

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                auto *o = GetOwnerBuffer();
                if (!o) return false;
                if (!binder.Bind("Pixel:gSceneTexture", o->GetSrvHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        // Pass3: composite internal blurred RT back to owner buffer
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.MotionBlur.HistoryCopy";
            pass.passName = "MotionBlur.BlitToOwner";
            pass.batchKey = 0;

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                if (!blurSrv_) return false;
                if (!binder.Bind("Pixel:gSceneTexture", blurSrv_->GetGPUDescriptorHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        return passes;
    }

private:
    struct CBData {
        float intensity;
        float velocityScale;
        float maxBlurPixels;
        std::uint32_t samples;
        float invResolution[2];
        float pad[2];
    };

    bool EnsureResources() const {
        auto *owner = GetOwnerBuffer();
        if (!owner) return false;

        const std::uint32_t w = owner->GetWidth();
        const std::uint32_t h = owner->GetHeight();
        if (w == 0 || h == 0) return false;

        DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (auto *rt = owner->GetRenderTarget()) {
            fmt = rt->GetFormat();
        }

        const bool needRecreate = (!historyRt_ || !historySrv_ || !blurRt_ || !blurSrv_ || rtW_ != w || rtH_ != h || fmt_ != fmt);
        if (needRecreate) {
            fmt_ = fmt;
            rtW_ = w;
            rtH_ = h;

            historyRt_ = std::make_unique<RenderTargetResource>(rtW_, rtH_, fmt_);
            historySrv_ = std::make_unique<ShaderResourceResource>(historyRt_.get());

            blurRt_ = std::make_unique<RenderTargetResource>(rtW_, rtH_, fmt_);
            blurSrv_ = std::make_unique<ShaderResourceResource>(blurRt_.get());
        }

        return historyRt_ && historySrv_ && blurRt_ && blurSrv_;
    }

    Params params_{};

    mutable ScreenBuffer* velocityBuffer_ = nullptr;

    mutable std::unique_ptr<RenderTargetResource> historyRt_;
    mutable std::unique_ptr<ShaderResourceResource> historySrv_;

    mutable std::unique_ptr<RenderTargetResource> blurRt_;
    mutable std::unique_ptr<ShaderResourceResource> blurSrv_;

    mutable std::uint32_t rtW_ = 0;
    mutable std::uint32_t rtH_ = 0;
    mutable DXGI_FORMAT fmt_ = DXGI_FORMAT_UNKNOWN;
};

} // namespace KashipanEngine
