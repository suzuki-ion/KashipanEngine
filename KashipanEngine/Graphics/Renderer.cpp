#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/ScreenBuffer.h"
#include <unordered_map>

namespace KashipanEngine {

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }

    // オフスクリーンの記録をまとめて行い、全オブジェクト描画後に一括 Close/Execute する
    ScreenBuffer::AllBeginRecord(Passkey<Renderer>{});
    RenderOffscreenPasses();
    {
        auto lists = ScreenBuffer::AllEndRecord(Passkey<Renderer>{});
        if (!lists.empty() && directXCommon_) {
            directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
        }
    }

    RenderScreenPasses();
    RenderPersistentPasses();
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

    std::vector<ID3D12CommandList *> lists;
    lists.reserve(persistentScreenPasses_.size());

    for (const auto *passInfo : persistentScreenPasses_) {
        if (!passInfo || !passInfo->buffer) continue;
        if (!passInfo->batchedRenderFunction) continue;

        auto *buffer = passInfo->buffer;
        const void *targetKey = static_cast<const void *>(buffer);

        auto *commandList = buffer->BeginRecord();
        if (!commandList) continue;

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

        if (!buffer->EndRecord(!ok)) {
            continue;
        }

        if (ok) {
            lists.push_back(commandList);
        }
    }

    if (!lists.empty() && directXCommon_) {
        directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
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
        } else {
            continue;
        }

        if (!passInfo->batchedRenderFunction || !passInfo->renderCommandFunction) continue;

        void *targetKeyVoid = getTargetKeyFunc ? getTargetKeyFunc(passInfo) : nullptr;
        const void *targetKey = static_cast<const void *>(targetKeyVoid);
        if (!targetKey) continue;

        ID3D12GraphicsCommandList *commandList = nullptr;
        PipelineBinder pipelineBinder;

        if (passInfo->screenBuffer) {
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

void Renderer::Render3DStandard(std::vector<const RenderPass *> renderPasses,
    std::function<void *(const RenderPass *)> getTargetKeyFunc) {
    // 2Dと同じ実装方針（シェーダ/パイプラインが変わるだけなので処理共通）
    Render2DStandard(std::move(renderPasses), std::move(getTargetKeyFunc));
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
        } else if (!first->screenBuffer) {
            continue;
        }

        if (!first->batchedRenderFunction || !first->renderCommandFunction || !first->submitInstanceFunction) {
            continue;
        }

        ID3D12GraphicsCommandList *commandList = nullptr;
        PipelineBinder pipelineBinder;
        ScreenBuffer *sb = nullptr;

        if (first->screenBuffer) {
            sb = first->screenBuffer;
            commandList = sb->BeginRecord();
            if (!commandList) continue;

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
            if (sb) sb->EndRecord(true);
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
            if (sb) sb->EndRecord(true);
            continue;
        }

        auto renderCommandOpt = first->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) {
            if (sb) sb->EndRecord(true);
            continue;
        }

        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);

        if (sb) {
            sb->EndRecord(false);
            std::vector<ID3D12CommandList *> lists;
            lists.push_back(commandList);
            if (directXCommon_) {
                directXCommon_->ExecuteExternalCommandLists(Passkey<Renderer>{}, lists);
            }
        }
    }
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

// DELETE these legacy definitions entirely (no longer declared in Renderer.h)
// void Renderer::RenderPasses2DStandardPersistent() { ... }
// void Renderer::RenderPasses3DStandardPersistent() { ... }

} // namespace KashipanEngine