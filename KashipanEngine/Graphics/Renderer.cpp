#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include <unordered_map>

namespace KashipanEngine {

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }

    {
        // ShadowMapBuffer の記録をフレーム内でまとめて行い、一括 Close/Execute する
        ShadowMapBuffer::AllBeginRecord(Passkey<Renderer>{});
        RenderShadowMapPasses();
        auto lists = ShadowMapBuffer::AllEndRecord(Passkey<Renderer>{});
        if (!lists.empty() && directXCommon_) {
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }
    {
        // ScreenBuffer の記録をフレーム内でまとめて行い、一括 Close/Execute する
        ScreenBuffer::AllBeginRecord(Passkey<Renderer>{});
        RenderOffscreenPasses();
        auto lists = ScreenBuffer::AllEndRecord(Passkey<Renderer>{});
        if (!lists.empty() && directXCommon_) {
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }

    RenderScreenPasses();
    RenderPersistentPasses();
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
        if (!passInfo || !passInfo->buffer) continue;
        if (!passInfo->batchedRenderFunction) continue;

        auto *buffer = passInfo->buffer;

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

        const void *targetKey = static_cast<const void *>(buffer);

        PipelineBinder pipelineBinder;
        pipelineBinder.SetManager(pipelineManager_);
        pipelineBinder.SetCommandList(commandList);
        pipelineBinder.Invalidate();
        pipelineBinder.UsePipeline(passInfo->pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo->pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        {
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

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

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

        if (!ok) {
            ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, buffer);
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
            std::vector<void *> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo->constantBufferRequirements.size());
            std::vector<ConstantBufferResource *> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo->constantBufferRequirements.size());

            for (const auto &req : passInfo->constantBufferRequirements) {
                ConstantBufferKey cbKey{ targetKey, passInfo->pipelineName, passInfo->batchKey, req.shaderNameKey, req.byteSize };
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

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

            if (ok) {
                for (const auto &req : passInfo->constantBufferRequirements) {
                    ConstantBufferKey cbKey{ targetKey, passInfo->pipelineName, passInfo->batchKey, req.shaderNameKey, req.byteSize };
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

                for (auto *b : mappedBuffers) {
                    if (b) b->Unmap();
                }
            }
        }

        if (!ok) {
            if (passInfo->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, passInfo->screenBuffer);
            }
            continue;
        }

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

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

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
            for (auto *b : mappedBuffers) {
                if (b) b->Unmap();
            }
            if (first->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, first->screenBuffer);
            }
            continue;
        }

        std::vector<void *> mapsTable = mappedPtrs;
        void *instanceMaps = mapsTable.empty() ? nullptr : mapsTable.data();

        for (std::uint32_t i = 0; i < instanceCount; ++i) {
            if (!itemsPtrs[i]->submitInstanceFunction(instanceMaps, shaderVariableBinder, i)) {
                ok = false;
                break;
            }
        }

        for (auto *b : mappedBuffers) {
            if (b) b->Unmap();
        }

        if (!ok) {
            if (first->screenBuffer) {
                ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, first->screenBuffer);
            }
            continue;
        }

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
    if (!pass.buffer) return {};
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

} // namespace KashipanEngine