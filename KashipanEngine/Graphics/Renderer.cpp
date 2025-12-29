#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include <unordered_map>

namespace KashipanEngine {

namespace {
struct PersistentBatchKey {
    HWND hwnd{};
    std::string pipelineName;
    std::uint64_t key = 0;

    bool operator==(const PersistentBatchKey &o) const {
        return hwnd == o.hwnd && key == o.key && pipelineName == o.pipelineName;
    }
};

struct PersistentBatchKeyHasher {
    size_t operator()(const PersistentBatchKey &k) const noexcept {
        size_t h1 = std::hash<void*>{}(k.hwnd);
        size_t h2 = std::hash<std::uint64_t>{}(k.key);
        size_t h3 = std::hash<std::string>{}(k.pipelineName);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2)) ^ (h3 << 1);
    }
};
} // namespace

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }

    RenderPasses3DStandardPersistent();
    RenderPasses3DInstancingPersistent();
    RenderPasses2DStandardPersistent();
    RenderPasses2DInstancingPersistent();
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
            BatchKey key{ p->window->GetWindowHandle(), p->pipelineName, p->batchKey };
            persistent2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            persistent3DStandard_.push_back(p);
        } else {
            BatchKey key{ p->window->GetWindowHandle(), p->pipelineName, p->batchKey };
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
            BatchKey key{ p->window->GetWindowHandle(), p->pipelineName, p->batchKey };
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
            BatchKey key{ p->window->GetWindowHandle(), p->pipelineName, p->batchKey };
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

void Renderer::RenderPasses2DStandardPersistent() {
    for (const auto *passInfo : persistent2DStandard_) {
        if (!passInfo || !passInfo->window ||
            passInfo->window->IsPendingDestroy() || passInfo->window->IsMinimized() || !passInfo->window->IsVisible()) {
            continue;
        }
        if (!passInfo->batchedRenderFunction || !passInfo->renderCommandFunction) {
            continue;
        }

        const HWND hwnd = passInfo->window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;

        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
        if (!commandList) continue;
        pipelineBinder.SetCommandList(commandList);

        pipelineBinder.UsePipeline(passInfo->pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo->pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo->constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo->constantBufferRequirements.size());

            for (const auto &req : passInfo->constantBufferRequirements) {
                ConstantBufferKey cbKey{ hwnd, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
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
                    ConstantBufferKey cbKey{ hwnd, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;
        ok = passInfo->batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        auto renderCommandOpt = passInfo->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = 1;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses3DStandardPersistent() {
    for (const auto *passInfo : persistent3DStandard_) {
        if (!passInfo || !passInfo->window ||
            passInfo->window->IsPendingDestroy() || passInfo->window->IsMinimized() || !passInfo->window->IsVisible()) {
            continue;
        }
        if (!passInfo->batchedRenderFunction || !passInfo->renderCommandFunction) {
            continue;
        }

        const HWND hwnd = passInfo->window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;

        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
        if (!commandList) continue;
        pipelineBinder.SetCommandList(commandList);

        pipelineBinder.UsePipeline(passInfo->pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo->pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo->constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo->constantBufferRequirements.size());

            for (const auto &req : passInfo->constantBufferRequirements) {
                ConstantBufferKey cbKey{ hwnd, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
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
                    ConstantBufferKey cbKey{ hwnd, passInfo->pipelineName, 0, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;
        ok = passInfo->batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        auto renderCommandOpt = passInfo->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = 1;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses2DInstancingPersistent() {
    if (persistent2DInstancing_.empty()) return;

    for (auto &kv : persistent2DInstancing_) {
        const auto &key = kv.first;
        auto &itemsPtrs = kv.second;
        if (itemsPtrs.empty()) continue;

        const RenderPass *first = itemsPtrs.front();
        Window *window = first ? first->window : nullptr;
        if (!window || window->IsPendingDestroy() || window->IsMinimized() || !window->IsVisible()) {
            continue;
        }

        auto it = windowBinders_.find(key.hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;

        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, key.hwnd);
        if (!commandList) continue;
        pipelineBinder.SetCommandList(commandList);

        pipelineBinder.UsePipeline(key.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, key.pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(itemsPtrs.size());
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(first->constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(first->constantBufferRequirements.size());

            for (const auto &req : first->constantBufferRequirements) {
                ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
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
                for (size_t i = 0; i < first->constantBufferRequirements.size(); ++i) {
                    const auto &req = first->constantBufferRequirements[i];
                    ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;
        ok = first->batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        std::vector<void*> mappedPtrs;
        mappedPtrs.reserve(first->instanceBufferRequirements.size());
        std::vector<StructuredBufferResource*> mappedBuffers;
        mappedBuffers.reserve(first->instanceBufferRequirements.size());

        for (const auto &req : first->instanceBufferRequirements) {
            InstanceBufferKey ibKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.elementStride };
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

        if (!ok) {
            for (auto *b : mappedBuffers) {
                if (b) b->Unmap();
            }
            continue;
        }

        std::vector<void*> mapsTable = mappedPtrs;
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

        if (!ok) continue;

        auto renderCommandOpt = first->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses3DInstancingPersistent() {
    if (persistent3DInstancing_.empty()) return;

    for (auto &kv : persistent3DInstancing_) {
        const auto &key = kv.first;
        auto &itemsPtrs = kv.second;
        if (itemsPtrs.empty()) continue;

        const RenderPass *first = itemsPtrs.front();
        Window *window = first ? first->window : nullptr;
        if (!window || window->IsPendingDestroy() || window->IsMinimized() || !window->IsVisible()) {
            continue;
        }

        auto it = windowBinders_.find(key.hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;

        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, key.hwnd);
        if (!commandList) continue;
        pipelineBinder.SetCommandList(commandList);

        pipelineBinder.UsePipeline(key.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, key.pipelineName);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(itemsPtrs.size());
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(first->constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(first->constantBufferRequirements.size());

            for (const auto &req : first->constantBufferRequirements) {
                ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
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
                for (size_t i = 0; i < first->constantBufferRequirements.size(); ++i) {
                    const auto &req = first->constantBufferRequirements[i];
                    ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;
        ok = first->batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        std::vector<void*> mappedPtrs;
        mappedPtrs.reserve(first->instanceBufferRequirements.size());
        std::vector<StructuredBufferResource*> mappedBuffers;
        mappedBuffers.reserve(first->instanceBufferRequirements.size());

        for (const auto &req : first->instanceBufferRequirements) {
            InstanceBufferKey ibKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.elementStride };
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

        if (!ok) {
            for (auto *b : mappedBuffers) {
                if (b) b->Unmap();
            }
            continue;
        }

        std::vector<void*> mapsTable = mappedPtrs;
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

        if (!ok) continue;

        auto renderCommandOpt = first->renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
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

void Renderer::RegisterRenderPass(const RenderPass &pass) {
    RenderPass copy = pass;
    (void)RegisterPersistentRenderPass(std::move(copy));
}

void Renderer::RegisterRenderPass(RenderPass &&pass) {
    (void)RegisterPersistentRenderPass(std::move(pass));
}

} // namespace KashipanEngine