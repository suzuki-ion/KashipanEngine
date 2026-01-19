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

class BloomEffect final : public IPostEffectComponent {
public:
    struct Params {
        float threshold = 1.0f;
        float softKnee = 0.5f;
        float intensity = 0.8f;
        float blurRadius = 1.0f;
        std::uint32_t iterations = 4; // ダウンサンプル段数 (1..16)
    };

    explicit BloomEffect(Params p = {})
        : IPostEffectComponent("BloomEffect", 1), params_(p) {}

    std::optional<bool> Initialize() override {
        return EnsureIntermediateTargets();
    }

    void SetParams(const Params &p) { params_ = p; }
    const Params &GetParams() const { return params_; }

    std::unique_ptr<IPostEffectComponent> Clone() const override {
        return std::make_unique<BloomEffect>(params_);
    }

#if defined(USE_IMGUI)
    void ShowImGui() override {
        ImGui::DragFloat("Threshold", &params_.threshold, 0.01f, 0.0f, 10.0f, "%.3f");
        ImGui::DragFloat("SoftKnee", &params_.softKnee, 0.01f, 0.0f, 1.0f, "%.3f");
        ImGui::DragFloat("Intensity", &params_.intensity, 0.01f, 0.0f, 5.0f, "%.3f");
        ImGui::DragFloat("BlurRadius", &params_.blurRadius, 0.01f, 0.0f, 10.0f, "%.3f");
        int it = static_cast<int>(params_.iterations);
        if (ImGui::DragInt("Iterations", &it, 1.0f, 1, 16)) {
            it = std::clamp(it, 1, 16);
            params_.iterations = static_cast<std::uint32_t>(it);
        }
    }
#endif

    std::vector<PostEffectPass> BuildPostEffectPasses() const override {
        auto *owner = GetOwnerBuffer();
        if (!owner) return {};
        if (!EnsureIntermediateTargets()) return {};

        const std::uint32_t levels = std::clamp<std::uint32_t>(params_.iterations, 1u, 16u);

        std::vector<PostEffectPass> passes;
        // Prefilter + blur0 + (levels-1)*(downsample+blur) + (levels-1)*upsample + composite
        passes.reserve(static_cast<size_t>(levels) * 3u + 2u);

        auto fillCB = [this](void *constantBufferMaps) -> bool {
            if (!constantBufferMaps) return false;
            void **maps = static_cast<void **>(constantBufferMaps);
            auto *cb = reinterpret_cast<CBData *>(maps[0]);
            if (!cb) return false;
            cb->threshold = params_.threshold;
            cb->softKnee = params_.softKnee;
            cb->intensity = params_.intensity;
            cb->blurRadius = params_.blurRadius;
            return true;
        };

        auto makeBeginForRT = [](LevelRT &dst) {
            return [&dst](ID3D12GraphicsCommandList *cl) -> bool {
                if (!cl) return false;
                if (!dst.rt) return false;

                dst.rt->SetCommandList(cl);
                if (!dst.rt->TransitionTo(D3D12_RESOURCE_STATE_RENDER_TARGET)) return false;

                const auto rtv = dst.rt->GetCPUDescriptorHandle();
                cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
                dst.rt->ClearRenderTargetView();

                D3D12_VIEWPORT vp{};
                vp.TopLeftX = 0.0f;
                vp.TopLeftY = 0.0f;
                vp.Width = static_cast<float>(dst.w);
                vp.Height = static_cast<float>(dst.h);
                vp.MinDepth = 0.0f;
                vp.MaxDepth = 1.0f;

                D3D12_RECT sc{};
                sc.left = 0;
                sc.top = 0;
                sc.right = static_cast<LONG>(dst.w);
                sc.bottom = static_cast<LONG>(dst.h);

                cl->RSSetViewports(1, &vp);
                cl->RSSetScissorRects(1, &sc);
                return true;
            };
        };

        auto makeEndForRT = [](LevelRT &dst) {
            return [&dst](ID3D12GraphicsCommandList *cl) -> bool {
                if (!cl) return false;
                if (!dst.rt) return false;
                dst.rt->SetCommandList(cl);
                return dst.rt->TransitionTo(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            };
        };

        // Prefilter: Scene -> pyramid_[0]
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.Bloom.Prefilter";
            pass.passName = "Bloom.Prefilter";
            pass.batchKey = 0;

            pass.constantBufferRequirements = { {"Pixel:BloomCB", sizeof(CBData)} };
            pass.updateConstantBuffersFunction = [fillCB](void *maps, std::uint32_t) -> bool { return fillCB(maps); };

            pass.beginRecordFunction = makeBeginForRT(pyramid_[0]);
            pass.endRecordFunction = makeEndForRT(pyramid_[0]);

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                auto *o = GetOwnerBuffer();
                if (!o) return false;
                if (!binder.Bind("Pixel:gTexture", o->GetSrvHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        // Blur level0: pyramid_[0] -> pyramidBlur_[0]
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.Bloom.Blur";
            pass.passName = "Bloom.Blur.L0";
            pass.batchKey = 0;

            pass.constantBufferRequirements = { {"Pixel:BloomCB", sizeof(CBData)} };
            pass.updateConstantBuffersFunction = [fillCB](void *maps, std::uint32_t) -> bool { return fillCB(maps); };

            pass.beginRecordFunction = makeBeginForRT(pyramidBlur_[0]);
            pass.endRecordFunction = makeEndForRT(pyramidBlur_[0]);

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                if (pyramid_.empty() || !pyramid_[0].srv) return false;
                if (!binder.Bind("Pixel:gTexture", pyramid_[0].srv->GetGPUDescriptorHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        // Downsample + Blur chain
        for (std::uint32_t i = 1; i < levels; ++i) {
            // Downsample: pyramidBlur_[i-1] -> pyramid_[i]
            {
                PostEffectPass pass;
                pass.pipelineName = "PostEffect.Bloom.Downsample";
                pass.passName = "Bloom.Downsample";
                pass.batchKey = i;

                pass.beginRecordFunction = makeBeginForRT(pyramid_[i]);
                pass.endRecordFunction = makeEndForRT(pyramid_[i]);

                pass.batchedRenderFunction = [this, i](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                    if (i == 0 || i > pyramidBlur_.size() - 1) return false;
                    auto &src = pyramidBlur_[i - 1];
                    if (!src.srv) return false;
                    if (!binder.Bind("Pixel:gTexture", src.srv->GetGPUDescriptorHandle())) return false;
                    if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                    return true;
                };

                pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                    return MakeDrawCommand(3);
                };

                passes.push_back(std::move(pass));
            }

            // Blur: pyramid_[i] -> pyramidBlur_[i]
            {
                PostEffectPass pass;
                pass.pipelineName = "PostEffect.Bloom.Blur";
                pass.passName = "Bloom.Blur";
                pass.batchKey = i;

                pass.constantBufferRequirements = { {"Pixel:BloomCB", sizeof(CBData)} };
                pass.updateConstantBuffersFunction = [fillCB](void *maps, std::uint32_t) -> bool { return fillCB(maps); };

                pass.beginRecordFunction = makeBeginForRT(pyramidBlur_[i]);
                pass.endRecordFunction = makeEndForRT(pyramidBlur_[i]);

                pass.batchedRenderFunction = [this, i](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                    if (i >= pyramid_.size()) return false;
                    auto &src = pyramid_[i];
                    if (!src.srv) return false;
                    if (!binder.Bind("Pixel:gTexture", src.srv->GetGPUDescriptorHandle())) return false;
                    if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                    return true;
                };

                pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                    return MakeDrawCommand(3);
                };

                passes.push_back(std::move(pass));
            }
        }

        // Upsample accumulate: from deep to 0
        // Start: accum[last] = blur[last]
        {
            const std::uint32_t last = levels - 1;
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.Bloom.Upsample";
            pass.passName = "Bloom.Accum.Init";
            pass.batchKey = last;

            pass.beginRecordFunction = makeBeginForRT(pyramidAccum_[last]);
            pass.endRecordFunction = makeEndForRT(pyramidAccum_[last]);

            pass.batchedRenderFunction = [this, last](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                if (!pyramidBlur_[last].srv) return false;
                // low = blur[last], high = black (use same as low for now)
                if (!binder.Bind("Pixel:gLowTexture", pyramidBlur_[last].srv->GetGPUDescriptorHandle())) return false;
                if (!binder.Bind("Pixel:gHighTexture", pyramidBlur_[last].srv->GetGPUDescriptorHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));
        }

        for (std::uint32_t i = levels - 1; i >= 1; --i) {
            const std::uint32_t dst = i - 1;
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.Bloom.Upsample";
            pass.passName = "Bloom.Upsample";
            pass.batchKey = dst;

            pass.beginRecordFunction = makeBeginForRT(pyramidAccum_[dst]);
            pass.endRecordFunction = makeEndForRT(pyramidAccum_[dst]);

            pass.batchedRenderFunction = [this, i, dst](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                if (!pyramidAccum_[i].srv) return false;
                if (!pyramidBlur_[dst].srv) return false;

                if (!binder.Bind("Pixel:gLowTexture", pyramidAccum_[i].srv->GetGPUDescriptorHandle())) return false;
                if (!binder.Bind("Pixel:gHighTexture", pyramidBlur_[dst].srv->GetGPUDescriptorHandle())) return false;
                if (!SamplerManager::BindSampler(&binder, "Pixel:gSampler", DefaultSampler::LinearClamp)) return false;
                return true;
            };

            pass.renderCommandFunction = [](PipelineBinder &) -> std::optional<RenderCommand> {
                return MakeDrawCommand(3);
            };

            passes.push_back(std::move(pass));

            if (i == 1) break;
        }

        // Composite: Scene + pyramidAccum_[0]
        {
            PostEffectPass pass;
            pass.pipelineName = "PostEffect.Bloom.Composite";
            pass.passName = "Bloom.Composite";
            pass.batchKey = 0;

            pass.constantBufferRequirements = { {"Pixel:BloomCB", sizeof(CBData)} };
            pass.updateConstantBuffersFunction = [fillCB](void *maps, std::uint32_t) -> bool { return fillCB(maps); };

            pass.batchedRenderFunction = [this](ShaderVariableBinder &binder, std::uint32_t) -> bool {
                auto *o = GetOwnerBuffer();
                if (!o) return false;
                if (pyramidAccum_.empty() || !pyramidAccum_[0].srv) return false;

                if (!binder.Bind("Pixel:gSceneTexture", o->GetSrvHandle())) return false;
                if (!binder.Bind("Pixel:gBloomTexture", pyramidAccum_[0].srv->GetGPUDescriptorHandle())) return false;
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
    struct LevelRT {
        std::unique_ptr<RenderTargetResource> rt;
        std::unique_ptr<ShaderResourceResource> srv;
        std::uint32_t w = 0;
        std::uint32_t h = 0;
    };

    bool EnsureIntermediateTargets() const {
        auto *owner = GetOwnerBuffer();
        if (!owner) return false;

        const std::uint32_t baseW = owner->GetWidth();
        const std::uint32_t baseH = owner->GetHeight();
        if (baseW == 0 || baseH == 0) return false;

        DXGI_FORMAT fmt = DXGI_FORMAT_B8G8R8A8_UNORM;
        if (auto *rt = owner->GetRenderTarget()) {
            fmt = rt->GetFormat();
        }

        const std::uint32_t levels = std::clamp<std::uint32_t>(params_.iterations, 1u, 16u);

        const bool needResize = (pyramidFmt_ != fmt || pyramid_.size() != levels || pyramidBlur_.size() != levels || pyramidAccum_.size() != levels);
        if (needResize) {
            pyramidFmt_ = fmt;
            pyramid_.clear();
            pyramid_.resize(levels);
            pyramidBlur_.clear();
            pyramidBlur_.resize(levels);
            pyramidAccum_.clear();
            pyramidAccum_.resize(levels);
        }

        std::uint32_t w = std::max(1u, baseW / 2u);
        std::uint32_t h = std::max(1u, baseH / 2u);

        for (std::uint32_t i = 0; i < levels; ++i) {
            auto ensureLevel = [&](LevelRT &lv) {
                const bool needRecreate = (!lv.rt || !lv.srv || lv.w != w || lv.h != h);
                if (needRecreate) {
                    lv.rt = std::make_unique<RenderTargetResource>(w, h, fmt);
                    lv.srv = std::make_unique<ShaderResourceResource>(lv.rt.get());
                    lv.w = w;
                    lv.h = h;
                }
            };

            ensureLevel(pyramid_[i]);
            ensureLevel(pyramidBlur_[i]);
            ensureLevel(pyramidAccum_[i]);

            w = std::max(1u, w / 2u);
            h = std::max(1u, h / 2u);
        }

        return !pyramid_.empty() && pyramid_[0].rt && pyramid_[0].srv && pyramidBlur_[0].rt && pyramidBlur_[0].srv && pyramidAccum_[0].rt && pyramidAccum_[0].srv;
    }

    struct CBData {
        float threshold;
        float softKnee;
        float intensity;
        float blurRadius;
    };

    Params params_{};

    mutable std::vector<LevelRT> pyramid_;
    mutable std::vector<LevelRT> pyramidBlur_;
    mutable std::vector<LevelRT> pyramidAccum_;
    mutable DXGI_FORMAT pyramidFmt_ = DXGI_FORMAT_UNKNOWN;
};

} // namespace KashipanEngine
