#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include <unordered_map>
#include <chrono>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {
using Clock = std::chrono::high_resolution_clock;
}

class RendererCpuTimerScope final {
public:
    RendererCpuTimerScope(Renderer &r, Renderer::CpuTimerStats::Scope scope) noexcept
        : r_(r), scope_(scope), begin_(Clock::now()) {}
    ~RendererCpuTimerScope() {
        const auto end = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - begin_).count();
        r_.AddCpuTimerSample_(scope_, ms);
    }

    RendererCpuTimerScope(const RendererCpuTimerScope &) = delete;
    RendererCpuTimerScope &operator=(const RendererCpuTimerScope &) = delete;

private:
    Renderer &r_;
    Renderer::CpuTimerStats::Scope scope_;
    Clock::time_point begin_;
};

namespace {
static const char *ToLabel(Renderer::CpuTimerStats::Scope s) {
    using S = Renderer::CpuTimerStats::Scope;
    switch (s) {
    case S::RenderFrame: return "RenderFrame";
    case S::ShadowMap_AllBeginRecord: return "ShadowMap AllBeginRecord";
    case S::ShadowMap_Passes: return "ShadowMap Passes";
    case S::ShadowMap_AllEndRecord: return "ShadowMap AllEndRecord";
    case S::ShadowMap_Execute: return "ShadowMap Execute";
    case S::Offscreen_AllBeginRecord: return "Offscreen AllBeginRecord";
    case S::Offscreen_Passes: return "Offscreen Passes";
    case S::Offscreen_AllEndRecord: return "Offscreen AllEndRecord";
    case S::Offscreen_Execute: return "Offscreen Execute";
    case S::PostEffect_AllBeginRecord: return "PostEffect AllBeginRecord";
    case S::PostEffect_Passes: return "PostEffect Passes";
    case S::PostEffect_AllEndRecord: return "PostEffect AllEndRecord";
    case S::PostEffect_Execute: return "PostEffect Execute";
    case S::Persistent_Passes: return "Persistent Passes";
    case S::Standard_Total: return "Standard Total";
    case S::Standard_ConstantBuffer_Update: return "Standard CB Update(Map/Unmap + update)";
    case S::Standard_ConstantBuffer_Bind: return "Standard CB Bind";
    case S::Standard_InstanceBuffer_Update: return "Standard IB Update(Map/Unmap + submit)";
    case S::Standard_RenderCommand: return "Standard RenderCommand + Draw";
    case S::Instancing_Total: return "Instancing Total";
    case S::Instancing_ConstantBuffer_Update: return "Instancing CB Update(Map/Unmap + update)";
    case S::Instancing_ConstantBuffer_Bind: return "Instancing CB Bind";
    case S::Instancing_InstanceBuffer_MapBind: return "Instancing IB Map/Bind";
    case S::Instancing_SubmitInstances: return "Instancing SubmitInstances(loop)";
    case S::Instancing_RenderCommand: return "Instancing RenderCommand + Draw";
    default: return "(unknown)";
    }
}
} // namespace

void Renderer::SetCpuTimerAverageWindow(std::uint32_t frames) noexcept {
    if (frames == 0) frames = 1;
    cpuTimerStats_.avgWindow = frames;
}

void Renderer::BeginCpuTimerFrame_() noexcept {
    // 現状はフレーム開始処理は無し（必要ならここでリセットや集計を実施）
}

void Renderer::AddCpuTimerSample_(CpuTimerStats::Scope scope, double ms) noexcept {
    const std::size_t idx = static_cast<std::size_t>(scope);
    if (idx >= cpuTimerStats_.samples.size()) return;

    auto &s = cpuTimerStats_.samples[idx];
    s.lastMs = ms;
    ++s.count;

    const double w = static_cast<double>(cpuTimerStats_.avgWindow == 0 ? 1u : cpuTimerStats_.avgWindow);
    if (s.count == 1) {
        s.avgMs = ms;
    } else {
        // EMA（window相当）で軽量に平均化
        const double alpha = 2.0 / (w + 1.0);
        s.avgMs = s.avgMs + alpha * (ms - s.avgMs);
    }
}

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    BeginCpuTimerFrame_();
    RendererCpuTimerScope tFrame(*this, CpuTimerStats::Scope::RenderFrame);

    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }

    {
        // ShadowMapBuffer の記録をフレーム内でまとめて行い、一括 Close/Execute する
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::ShadowMap_AllBeginRecord);
            ShadowMapBuffer::AllBeginRecord(Passkey<Renderer>{});
        }
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::ShadowMap_Passes);
            RenderShadowMapPasses();
        }
        decltype(ShadowMapBuffer::AllEndRecord(Passkey<Renderer>{})) lists;
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::ShadowMap_AllEndRecord);
            lists = ShadowMapBuffer::AllEndRecord(Passkey<Renderer>{});
        }
        if (!lists.empty() && directXCommon_) {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::ShadowMap_Execute);
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }
    {
        // ScreenBuffer の記録をフレーム内でまとめて行い、一括 Close/Execute する
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::Offscreen_AllBeginRecord);
            ScreenBuffer::AllBeginRecord(Passkey<Renderer>{});
        }
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::Offscreen_Passes);
            RenderOffscreenPasses();
        }
        decltype(ScreenBuffer::AllEndRecord(Passkey<Renderer>{})) lists;
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::Offscreen_AllEndRecord);
            lists = ScreenBuffer::AllEndRecord(Passkey<Renderer>{});
        }
        if (!lists.empty() && directXCommon_) {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::Offscreen_Execute);
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }
    {
        // ポストエフェクトコンポーネントの更新
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::PostEffect_AllBeginRecord);
            ScreenBuffer::AllBeginRecord(Passkey<Renderer>{});
        }
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::PostEffect_Passes);
            RenderScreenPasses();
        }
        decltype(ScreenBuffer::AllEndRecord(Passkey<Renderer>{})) lists;
        {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::PostEffect_AllEndRecord);
            lists = ScreenBuffer::AllEndRecord(Passkey<Renderer>{});
        }
        if (!lists.empty() && directXCommon_) {
            RendererCpuTimerScope t(*this, CpuTimerStats::Scope::PostEffect_Execute);
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }
    {
        RendererCpuTimerScope t(*this, CpuTimerStats::Scope::Persistent_Passes);
        RenderPersistentPasses();
    }
}

Renderer::PersistentShadowMapPassHandle Renderer::RegisterPersistentShadowMapRenderPass(RenderPass &&pass) {
    if (!pass.shadowMapBuffer) return {};

    PersistentShadowMapPassHandle handle{ nextPersistentShadowMapPassId_++ };
    PersistentShadowMapPassEntry entry{ handle, std::move(pass) };
    auto [it, inserted] = persistentShadowMapPassesById_.emplace(handle.id, std::move(entry));
    if (!inserted) return {};

    const RenderPass *p = &it->second.pass;

    const void *targetKey = static_cast<const void*>(p->shadowMapBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            shadowMap2DStandard_.push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            shadowMap2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            shadowMap3DStandard_.push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            shadowMap3DInstancing_[key].push_back(p);
        }
    }

    return handle;
}

bool Renderer::UnregisterPersistentShadowMapRenderPass(PersistentShadowMapPassHandle handle) {
    if (!handle) return false;
    auto it = persistentShadowMapPassesById_.find(handle.id);
    if (it == persistentShadowMapPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    const void *targetKey = static_cast<const void*>(p->shadowMapBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(shadowMap2DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto itB = shadowMap2DInstancing_.find(key);
            if (itB != shadowMap2DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) shadowMap2DInstancing_.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(shadowMap3DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto itB = shadowMap3DInstancing_.find(key);
            if (itB != shadowMap3DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) shadowMap3DInstancing_.erase(itB);
            }
        }
    }

    persistentShadowMapPassesById_.erase(it);
    return true;
}

void Renderer::RenderShadowMapPasses() {
    if (shadowMap2DStandard_.empty() && shadowMap3DStandard_.empty() &&
        shadowMap2DInstancing_.empty() && shadowMap3DInstancing_.empty()) {
        return;
    }

    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->shadowMapBuffer) return nullptr;
        return const_cast<void *>(static_cast<const void *>(passInfo->shadowMapBuffer));
    };

    Render3DStandard(shadowMap3DStandard_, getTargetKey);
    Render3DInstancing(shadowMap3DInstancing_, getTargetKey);
    Render2DStandard(shadowMap2DStandard_, getTargetKey);
    Render2DInstancing(shadowMap2DInstancing_, getTargetKey);
}

void Renderer::RenderOffscreenPasses() {
    if (offscreen2DStandard_.empty() && offscreen3DStandard_.empty() &&
        offscreen2DInstancing_.empty() && offscreen3DInstancing_.empty()) {
        return;
    }

    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->screenBuffer) return nullptr;
        return const_cast<void *>(static_cast<const void *>(passInfo->screenBuffer));
    };

    Render3DStandard(offscreen3DStandard_, getTargetKey);
    Render3DInstancing(offscreen3DInstancing_, getTargetKey);
    Render2DStandard(offscreen2DStandard_, getTargetKey);
    Render2DInstancing(offscreen2DInstancing_, getTargetKey);
}

void Renderer::RenderScreenPasses() {
    if (persistentScreenPasses_.empty()) return;

    for (const auto *passInfo : persistentScreenPasses_) {
        if (!passInfo || !passInfo->screenBuffer) continue;

        auto *buffer = passInfo->screenBuffer;

        // ScreenBuffer は RenderFrame() の AllBeginRecord で既に Reset/RT セット済み
        if (!buffer->IsRecording(Passkey<Renderer>{})) {
            ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
            continue;
        }

        auto *commandList = buffer->GetRecordedCommandList(Passkey<Renderer>{});
        if (!commandList) {
            ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
            continue;
        }

        // PostEffect の各パスを、この ScreenBuffer の commandList 上で個別に実行
        auto effectPasses = buffer->BuildPostEffectPasses(Passkey<Renderer>{});
        if (effectPasses.empty()) {
            // 何もない場合も正常。ScreenBuffer の offscreen 描画結果だけが作られる。
            continue;
        }

        const void *targetKey = static_cast<const void *>(buffer);

        for (const auto &fx : effectPasses) {
            if (fx.pipelineName.empty()) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
                break;
            }

            PipelineBinder pipelineBinder;
            pipelineBinder.SetManager(pipelineManager_);
            pipelineBinder.SetCommandList(commandList);
            pipelineBinder.Invalidate();
            pipelineBinder.UsePipeline(fx.pipelineName);

            auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, fx.pipelineName);
            shaderVariableBinder.SetCommandList(commandList);

            const std::uint32_t instanceCount = 1;
            bool ok = true;

            // Constant buffers
            {
                std::vector<void *> cbMappedPtrs;
                cbMappedPtrs.reserve(fx.constantBufferRequirements.size());

                for (const auto &req : fx.constantBufferRequirements) {
                    ConstantBufferKey cbKey{ targetKey, fx.pipelineName, fx.batchKey, req.shaderNameKey, req.byteSize };
                    auto &entry = constantBuffers_[cbKey];
                    if (!entry.buffer || entry.byteSize != req.byteSize) {
                        entry.byteSize = req.byteSize;
                        entry.buffer = std::make_unique<ConstantBufferResource>(req.byteSize);
                    }
                    void *p = entry.buffer->Map();
                    cbMappedPtrs.push_back(p);
                }

                void *constantBufferMaps = cbMappedPtrs.empty() ? nullptr : cbMappedPtrs.data();
                if (fx.updateConstantBuffersFunction) {
                    ok = ok && fx.updateConstantBuffersFunction(constantBufferMaps, instanceCount);
                }

                if (ok) {
                    for (const auto &req : fx.constantBufferRequirements) {
                        ConstantBufferKey cbKey{ targetKey, fx.pipelineName, fx.batchKey, req.shaderNameKey, req.byteSize };
                        auto itCB = constantBuffers_.find(cbKey);
                        if (itCB == constantBuffers_.end() || !itCB->second.buffer) {
                            ok = false;
                            break;
                        }
                        ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                        if (!ok) break;
                    }
                }
            }

            // Effect-specific resource binds
            if (ok && fx.batchedRenderFunction) {
                ok = ok && fx.batchedRenderFunction(shaderVariableBinder, instanceCount);
            }

            // Instance buffers (optional)
            std::vector<void *> mappedPtrs;
            if (ok && !fx.instanceBufferRequirements.empty()) {
                if (!fx.submitInstanceFunction) {
                    ok = false;
                } else {
                    mappedPtrs.reserve(fx.instanceBufferRequirements.size());

                    for (const auto &req : fx.instanceBufferRequirements) {
                        InstanceBufferKey ibKey{ targetKey, fx.pipelineName, fx.batchKey, req.shaderNameKey, req.elementStride };
                        auto &entry = instanceBuffers_[ibKey];
                        if (entry.capacity < instanceCount || !entry.buffer) {
                            entry.capacity = instanceCount;
                            entry.buffer = std::make_unique<StructuredBufferResource>(req.elementStride, entry.capacity);
                        }

                        ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, entry.buffer->GetGPUDescriptorHandle());
                        if (!ok) break;

                        void *p = entry.buffer->Map();
                        mappedPtrs.push_back(p);
                    }

                    if (ok) {
                        void *instanceMaps = mappedPtrs.empty() ? nullptr : mappedPtrs.data();
                        ok = ok && fx.submitInstanceFunction(instanceMaps, shaderVariableBinder, 0);
                    }
                }
            }

            if (!ok || !fx.renderCommandFunction) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
                break;
            }

            auto renderCommandOpt = fx.renderCommandFunction(pipelineBinder);
            if (!renderCommandOpt) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
                break;
            }

            RenderCommand cmd;
            cmd.vertexCount = renderCommandOpt->vertexCount;
            cmd.indexCount = renderCommandOpt->indexCount;
            cmd.instanceCount = 1;
            cmd.startVertexLocation = renderCommandOpt->startVertexLocation;
            cmd.startIndexLocation = renderCommandOpt->startIndexLocation;
            cmd.baseVertexLocation = renderCommandOpt->baseVertexLocation;
            cmd.startInstanceLocation = 0;
            IssueRenderCommand(commandList, cmd);
        }
    }
}

void Renderer::RenderPersistentPasses() {
    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->window) return nullptr;
        const HWND hwnd = passInfo->window->GetWindowHandle();
        return const_cast<void *>(static_cast<const void *>(hwnd));
    };

    Render3DStandard(persistent3DStandard_, getTargetKey);
    Render3DInstancing(persistent3DInstancing_, getTargetKey);
    Render2DStandard(persistent2DStandard_, getTargetKey);
    Render2DInstancing(persistent2DInstancing_, getTargetKey);
}

void Renderer::Render2DStandard(std::vector<const RenderPass *> renderPasses,
     std::function<void *(const RenderPass *)> getTargetKeyFunc) {
    RendererCpuTimerScope tTotal(*this, CpuTimerStats::Scope::Standard_Total);
    for (const auto *passInfo : renderPasses) {
        if (!passInfo) continue;

        if (passInfo->window) {
            if (passInfo->window->IsPendingDestroy() || passInfo->window->IsMinimized() || !passInfo->window->IsVisible()) {
                continue;
            }
        } else if (passInfo->screenBuffer) {
            // ok
        } else if (passInfo->shadowMapBuffer) {
            // ok
        } else {
            continue;
        }

        if (!passInfo->batchedRenderFunction || !passInfo->renderCommandFunction) continue;

        void *targetKeyVoid = getTargetKeyFunc ? getTargetKeyFunc(passInfo) : nullptr;
        const void *targetKey = static_cast<const void *>(targetKeyVoid);
        if (!targetKey) continue;

        ID3D12GraphicsCommandList *commandList = nullptr;
        PipelineBinder pipelineBinder;

        if (passInfo->shadowMapBuffer) {
            ShadowMapBuffer *smb = passInfo->shadowMapBuffer;

            if (!smb->IsRecording(Passkey<Renderer>{})) {
                continue;
            }

            commandList = smb->GetRecordedCommandList(Passkey<Renderer>{});
            if (!commandList) {
                continue;
            }

            pipelineBinder.SetManager(pipelineManager_);
            pipelineBinder.SetCommandList(commandList);
            pipelineBinder.Invalidate();
        } else if (passInfo->screenBuffer) {
            ScreenBuffer *sb = passInfo->screenBuffer;

            // ScreenBuffer は RenderFrame() の AllBeginRecord で既に Reset/RT セット済み
            if (!sb->IsRecording(Passkey<Renderer>{})) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, sb);
                continue;
            }

            commandList = sb->GetRecordedCommandList(Passkey<Renderer>{});
            if (!commandList) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, sb);
                continue;
            }

            pipelineBinder.SetManager(pipelineManager_);
            pipelineBinder.SetCommandList(commandList);
            pipelineBinder.Invalidate();
        } else {
            if (!directXCommon_) continue;

            const HWND hwnd = passInfo->window->GetWindowHandle();
            auto it = windowBinders_.find(hwnd);
            if (it == windowBinders_.end()) continue;

            commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
            if (!commandList) continue;

            pipelineBinder = it->second;
            pipelineBinder.SetCommandList(commandList);
        }

        pipelineBinder.UsePipeline(passInfo->pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo->pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        // Constant buffers (per pass)
        {
            RendererCpuTimerScope tCbUpdate(*this, CpuTimerStats::Scope::Standard_ConstantBuffer_Update);
            std::vector<void *> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo->constantBufferRequirements.size());
            std::vector<ConstantBufferResource *> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo->constantBufferRequirements.size());

            for (const auto &req : passInfo->constantBufferRequirements) {
                ConstantBufferKey cbKey{ targetKey, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
                auto &entry = constantBuffers_[cbKey];
                if (!entry.buffer || entry.byteSize != req.byteSize) {
                    entry.byteSize = req.byteSize;
                    entry.buffer = std::make_unique<ConstantBufferResource>(req.byteSize);
                }
                void *p = entry.buffer->Map();
                cbMappedPtrs.push_back(p);
                cbMappedBuffers.push_back(entry.buffer.get());
            }

            void *constantBufferMaps = cbMappedPtrs.empty() ? nullptr : cbMappedPtrs.data();
            if (passInfo->updateConstantBuffersFunction) {
                ok = ok && passInfo->updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            // 永続Mapのため Unmap は不要

            if (ok) {
                // Bind は update と分けて計測したいので、ここで update計測を閉じて Bind側を別スコープにする
            }
        }
        {
            RendererCpuTimerScope tCbBind(*this, CpuTimerStats::Scope::Standard_ConstantBuffer_Bind);
            if (ok) {
                for (const auto &req : passInfo->constantBufferRequirements) {
                    ConstantBufferKey cbKey{ targetKey, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) {
                        ok = false;
                        break;
                    }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (ok) {
            ok = passInfo->batchedRenderFunction(shaderVariableBinder, instanceCount);
        }

        // Instance buffers (per pass, instanceCount = 1)
        std::vector<void *> mappedPtrs;
        std::vector<StructuredBufferResource *> mappedBuffers;
        if (ok && !passInfo->instanceBufferRequirements.empty()) {
            RendererCpuTimerScope tIbUpdate(*this, CpuTimerStats::Scope::Standard_InstanceBuffer_Update);
            if (!passInfo->submitInstanceFunction) {
                ok = false;
            } else {
                mappedPtrs.reserve(passInfo->instanceBufferRequirements.size());
                mappedBuffers.reserve(passInfo->instanceBufferRequirements.size());

                for (const auto &req : passInfo->instanceBufferRequirements) {
                    InstanceBufferKey ibKey{ targetKey, passInfo->pipelineName, passInfo->batchKey, req.shaderNameKey, req.elementStride };
                    auto &entry = instanceBuffers_[ibKey];
                    if (entry.capacity < instanceCount || !entry.buffer) {
                        entry.capacity = instanceCount;
                        entry.buffer = std::make_unique<StructuredBufferResource>(req.elementStride, entry.capacity);
                    }

                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, entry.buffer->GetGPUDescriptorHandle());
                    if (!ok) break;

                    void *p = entry.buffer->Map();
                    mappedPtrs.push_back(p);
                    mappedBuffers.push_back(entry.buffer.get());
                }

                if (ok) {
                    void *instanceMaps = mappedPtrs.empty() ? nullptr : mappedPtrs.data();
                    ok = ok && passInfo->submitInstanceFunction(instanceMaps, shaderVariableBinder, 0);
                }

                // 永続Mapのため Unmap は不要
            }
        }

        if (!ok) {
            if (passInfo->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, passInfo->screenBuffer);
            }
            continue;
        }

        RendererCpuTimerScope tCmd(*this, CpuTimerStats::Scope::Standard_RenderCommand);
        auto renderCommandOpt = passInfo->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) {
            if (passInfo->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, passInfo->screenBuffer);
            }
            continue;
        }

        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = 1;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::Render2DInstancing(std::unordered_map<BatchKey, std::vector<const RenderPass *>, BatchKeyHasher> &renderPasses,
     std::function<void *(const RenderPass *)> /*getTargetKeyFunc*/) {
    RendererCpuTimerScope tTotal(*this, CpuTimerStats::Scope::Instancing_Total);

    for (auto &kv : renderPasses) {
        const auto &key = kv.first;
        auto &itemsPtrs = kv.second;
        if (itemsPtrs.empty()) continue;

        const RenderPass *first = itemsPtrs.front();
        if (!first) continue;

        if (first->window) {
            Window *window = first->window;
            if (window->IsPendingDestroy() || window->IsMinimized() || !window->IsVisible()) {
                continue;
            }
        } else if (first->screenBuffer) {
            // ok
        } else if (first->shadowMapBuffer) {
            // ok
        } else {
            continue;
        }

        if (!first->batchedRenderFunction || !first->renderCommandFunction || !first->submitInstanceFunction) {
            continue;
        }

        ID3D12GraphicsCommandList *commandList = nullptr;
        PipelineBinder pipelineBinder;

        if (first->shadowMapBuffer) {
            ShadowMapBuffer *smb = first->shadowMapBuffer;

            if (!smb->IsRecording(Passkey<Renderer>{})) {
                continue;
            }

            commandList = smb->GetRecordedCommandList(Passkey<Renderer>{});
            if (!commandList) {
                continue;
            }

            pipelineBinder.SetManager(pipelineManager_);
            pipelineBinder.SetCommandList(commandList);
            pipelineBinder.Invalidate();
        } else if (first->screenBuffer) {
            ScreenBuffer *sb = first->screenBuffer;

            // ScreenBuffer は RenderFrame() の AllBeginRecord で既に Reset/RT セット済み
            if (!sb->IsRecording(Passkey<Renderer>{})) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, sb);
                continue;
            }

            commandList = sb->GetRecordedCommandList(Passkey<Renderer>{});
            if (!commandList) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, sb);
                continue;
            }

            pipelineBinder.SetManager(pipelineManager_);
            pipelineBinder.SetCommandList(commandList);
            pipelineBinder.Invalidate();
        } else {
            if (!directXCommon_) continue;

            const HWND hwnd = first->window->GetWindowHandle();
            auto it = windowBinders_.find(hwnd);
            if (it == windowBinders_.end()) continue;

            commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
            if (!commandList) continue;

            pipelineBinder = it->second;
            pipelineBinder.SetCommandList(commandList);
        }

        pipelineBinder.UsePipeline(key.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, key.pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(itemsPtrs.size());
        bool ok = true;

        // Constant buffers (shared per batch)
        {
            RendererCpuTimerScope tCbUpdate(*this, CpuTimerStats::Scope::Instancing_ConstantBuffer_Update);
            std::vector<void *> cbMappedPtrs;
            cbMappedPtrs.reserve(first->constantBufferRequirements.size());
            std::vector<ConstantBufferResource *> cbMappedBuffers;
            cbMappedBuffers.reserve(first->constantBufferRequirements.size());

            for (const auto &req : first->constantBufferRequirements) {
                ConstantBufferKey cbKey{ key.targetKey, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                auto &entry = constantBuffers_[cbKey];
                if (!entry.buffer || entry.byteSize != req.byteSize) {
                    entry.byteSize = req.byteSize;
                    entry.buffer = std::make_unique<ConstantBufferResource>(req.byteSize);
                }
                void *p = entry.buffer->Map();
                cbMappedPtrs.push_back(p);
                cbMappedBuffers.push_back(entry.buffer.get());
            }

            void *constantBufferMaps = cbMappedPtrs.empty() ? nullptr : cbMappedPtrs.data();
            if (first->updateConstantBuffersFunction) {
                ok = ok && first->updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            // 永続Mapのため Unmap は不要

            if (ok) {
                // Bind は update と分けて計測したいので、ここで update計測を閉じて Bind側を別スコープにする
            }
        }
        {
            RendererCpuTimerScope tCbBind(*this, CpuTimerStats::Scope::Instancing_ConstantBuffer_Bind);
            if (ok) {
                for (const auto &req : first->constantBufferRequirements) {
                    ConstantBufferKey cbKey{ key.targetKey, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) {
                        ok = false;
                        break;
                    }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (ok) {
            ok = first->batchedRenderFunction(shaderVariableBinder, instanceCount);
        }

        // Instance buffers
        std::vector<void *> mappedPtrs;
        std::vector<StructuredBufferResource *> mappedBuffers;
        if (ok) {
            RendererCpuTimerScope tIb(*this, CpuTimerStats::Scope::Instancing_InstanceBuffer_MapBind);
            mappedPtrs.reserve(first->instanceBufferRequirements.size());
            mappedBuffers.reserve(first->instanceBufferRequirements.size());

            for (const auto &req : first->instanceBufferRequirements) {
                InstanceBufferKey ibKey{ key.targetKey, key.pipelineName, key.key, req.shaderNameKey, req.elementStride };
                auto &entry = instanceBuffers_[ibKey];
                if (entry.capacity < instanceCount || !entry.buffer) {
                    entry.capacity = instanceCount;
                    entry.buffer = std::make_unique<StructuredBufferResource>(req.elementStride, entry.capacity);
                }

                ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, entry.buffer->GetGPUDescriptorHandle());
                if (!ok) break;

                void *p = entry.buffer->Map();
                mappedPtrs.push_back(p);
                mappedBuffers.push_back(entry.buffer.get());
            }
        }

        if (!ok) {
            if (first->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, first->screenBuffer);
            }
            continue;
        }

        {
            std::vector<void *> mapsTable = mappedPtrs;
            void *instanceMaps = mapsTable.empty() ? nullptr : mapsTable.data();

            RendererCpuTimerScope tSubmit(*this, CpuTimerStats::Scope::Instancing_SubmitInstances);
            for (std::uint32_t i = 0; i < instanceCount; ++i) {
                if (!itemsPtrs[i]->submitInstanceFunction(instanceMaps, shaderVariableBinder, i)) {
                    ok = false;
                    break;
                }
            }
        }

        if (!ok) {
            if (first->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, first->screenBuffer);
            }
            continue;
        }

        RendererCpuTimerScope tCmd(*this, CpuTimerStats::Scope::Instancing_RenderCommand);
        auto renderCommandOpt = first->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) {
            if (first->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, first->screenBuffer);
            }
            continue;
        }

        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::Render3DStandard(std::vector<const RenderPass *> renderPasses,
    std::function<void *(const RenderPass *)> getTargetKeyFunc) {
    Render2DStandard(std::move(renderPasses), std::move(getTargetKeyFunc));
}

void Renderer::Render3DInstancing(std::unordered_map<BatchKey, std::vector<const RenderPass *>, BatchKeyHasher> &renderPasses,
    std::function<void *(const RenderPass *)> getTargetKeyFunc) {
    Render2DInstancing(renderPasses, std::move(getTargetKeyFunc));
}

Renderer::PersistentPassHandle Renderer::RegisterPersistentRenderPass(RenderPass &&pass) {
    if (!pass.window) return {};
    PersistentPassHandle handle{ nextPersistentPassId_++ };
    PersistentPassEntry entry{ handle, std::move(pass) };
    auto [it, inserted] = persistentPassesById_.emplace(handle.id, std::move(entry));
    if (!inserted) return {};

    const RenderPass *p = &it->second.pass;
    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            persistent2DStandard_.push_back(p);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            persistent2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            persistent3DStandard_.push_back(p);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            persistent3DInstancing_[key].push_back(p);
        }
    }
    return handle;
}

bool Renderer::UnregisterPersistentRenderPass(PersistentPassHandle handle) {
    if (!handle) return false;
    auto it = persistentPassesById_.find(handle.id);
    if (it == persistentPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(persistent2DStandard_);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            auto itB = persistent2DInstancing_.find(key);
            if (itB != persistent2DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) persistent2DInstancing_.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(persistent3DStandard_);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            auto itB = persistent3DInstancing_.find(key);
            if (itB != persistent3DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) persistent3DInstancing_.erase(itB);
            }
        }
    }

    persistentPassesById_.erase(it);
    return true;
}

Renderer::PersistentScreenPassHandle Renderer::RegisterPersistentScreenPass(ScreenBufferPass&& pass) {
    if (!pass.screenBuffer) return {};
    PersistentScreenPassHandle handle{ nextPersistentScreenPassId_++ };
    PersistentScreenPassEntry entry{ handle, std::move(pass) };
    auto [it, inserted] = persistentScreenPassesById_.emplace(handle.id, std::move(entry));
    if (!inserted) return {};

    const ScreenBufferPass* p = &it->second.pass;
    persistentScreenPasses_.push_back(p);
    return handle;
}

bool Renderer::UnregisterPersistentScreenPass(PersistentScreenPassHandle handle) {
    if (!handle) return false;
    auto it = persistentScreenPassesById_.find(handle.id);
    if (it == persistentScreenPassesById_.end()) return false;

    const ScreenBufferPass* p = &it->second.pass;
    for (auto vit = persistentScreenPasses_.begin(); vit != persistentScreenPasses_.end(); ++vit) {
        if (*vit == p) { persistentScreenPasses_.erase(vit); break; }
    }

    persistentScreenPassesById_.erase(it);
    return true;
}

Renderer::PersistentOffscreenPassHandle Renderer::RegisterPersistentOffscreenRenderPass(RenderPass &&pass) {
    if (!pass.screenBuffer) return {};

    PersistentOffscreenPassHandle handle{ nextPersistentOffscreenPassId_++ };
    PersistentOffscreenPassEntry entry{ handle, std::move(pass) };
    auto [it, inserted] = persistentOffscreenPassesById_.emplace(handle.id, std::move(entry));
    if (!inserted) return {};

    const RenderPass *p = &it->second.pass;
    const void *targetKey = static_cast<const void*>(p->screenBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            offscreen2DStandard_.push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            offscreen2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            offscreen3DStandard_.push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            offscreen3DInstancing_[key].push_back(p);
        }
    }

    return handle;
}

bool Renderer::UnregisterPersistentOffscreenRenderPass(PersistentOffscreenPassHandle handle) {
    if (!handle) return false;
    auto it = persistentOffscreenPassesById_.find(handle.id);
    if (it == persistentOffscreenPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    const void *targetKey = static_cast<const void*>(p->screenBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(offscreen2DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto itB = offscreen2DInstancing_.find(key);
            if (itB != offscreen2DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) offscreen2DInstancing_.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(offscreen3DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto itB = offscreen3DInstancing_.find(key);
            if (itB != offscreen3DInstancing_.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) offscreen3DInstancing_.erase(itB);
            }
        }
    }

    persistentOffscreenPassesById_.erase(it);
    return true;
}

void Renderer::RegisterWindow(Passkey<Window>, HWND hwnd, ID3D12GraphicsCommandList *commandList) {
    if (!hwnd) return;
    auto it = windowBinders_.find(hwnd);
    if (it == windowBinders_.end()) {
        PipelineBinder binder;
        binder.SetManager(pipelineManager_);
        if (commandList) binder.SetCommandList(commandList);
        binder.Invalidate();
        windowBinders_.emplace(hwnd, std::move(binder));
    }
}

void Renderer::IssueRenderCommand(ID3D12GraphicsCommandList *commandList, const RenderCommand &renderCommand) {
    if (!commandList) return;
    if (renderCommand.indexCount > 0) {
        commandList->DrawIndexedInstanced(
            renderCommand.indexCount,
            renderCommand.instanceCount,
            renderCommand.startIndexLocation,
            renderCommand.baseVertexLocation,
            renderCommand.startInstanceLocation
        );
    } else if (renderCommand.vertexCount > 0) {
        commandList->DrawInstanced(
            renderCommand.vertexCount,
            renderCommand.instanceCount,
            renderCommand.startVertexLocation,
            renderCommand.startInstanceLocation
        );
    }
}

#if defined(USE_IMGUI)
void Renderer::ShowImGuiCpuTimersWindow() {
    if (!ImGui::Begin("Renderer CPU Timers")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Avg Window (EMA): %u", cpuTimerStats_.avgWindow);
    if (ImGui::Button("AvgWindow 60")) SetCpuTimerAverageWindow(60);
    ImGui::SameLine();
    if (ImGui::Button("AvgWindow 120")) SetCpuTimerAverageWindow(120);
    ImGui::SameLine();
    if (ImGui::Button("AvgWindow 300")) SetCpuTimerAverageWindow(300);

    ImGui::Separator();

    if (ImGui::BeginTable("##RendererCpuTimers", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Scope");
        ImGui::TableSetupColumn("Last (ms)", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Avg (ms)", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableHeadersRow();

        for (std::size_t i = 0; i < cpuTimerStats_.samples.size(); ++i) {
            auto scope = static_cast<CpuTimerStats::Scope>(i);
            const auto &s = cpuTimerStats_.samples[i];

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(ToLabel(scope));
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.3f", static_cast<float>(s.lastMs));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.3f", static_cast<float>(s.avgMs));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%llu", static_cast<unsigned long long>(s.count));
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
#endif

} // namespace KashipanEngine