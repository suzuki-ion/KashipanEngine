#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include <unordered_map>

namespace KashipanEngine {

void Renderer::RenderFrame(Passkey<GraphicsEngine>) {
    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }
    RenderPasses3D();
    RenderPasses2D();

    renderPasses2DStandard_.clear();
    renderPasses3DStandard_.clear();
    renderPasses2DInstancing_.clear();
    renderPasses3DInstancing_.clear();
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

namespace {
struct BatchKey {
    HWND hwnd{};
    std::string pipelineName;
    std::uint64_t key = 0;

    bool operator==(const BatchKey& o) const {
        return hwnd == o.hwnd && key == o.key && pipelineName == o.pipelineName;
    }
};

struct BatchKeyHasher {
    size_t operator()(const BatchKey& k) const noexcept {
        size_t h1 = std::hash<void*>{}(k.hwnd);
        size_t h2 = std::hash<std::uint64_t>{}(k.key);
        size_t h3 = std::hash<std::string>{}(k.pipelineName);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2)) ^ (h3 << 1);
    }
};
} // namespace

void Renderer::RenderPasses2D() {
    RenderPasses2DStandard();
    RenderPasses2DInstancing();
}

void Renderer::RenderPasses3D() {
    RenderPasses3DStandard();
    RenderPasses3DInstancing();
}

void Renderer::RenderPasses2DInstancing() {
    if (renderPasses2DInstancing_.empty()) return;

    for (auto &kv : renderPasses2DInstancing_) {
        const auto &key = kv.first;
        auto &items = kv.second;
        if (items.empty()) continue;

        Window *window = items.front().window;
        if (!window ||
            window->IsPendingDestroy() || window->IsMinimized() || !window->IsVisible()) {
            continue;
        }

        auto it = windowBinders_.find(key.hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;
        pipelineBinder.UsePipeline(key.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, key.pipelineName);
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, key.hwnd);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(items.size());
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(items.front().constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(items.front().constantBufferRequirements.size());

            for (const auto &req : items.front().constantBufferRequirements) {
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
            if (items.front().updateConstantBuffersFunction) {
                ok = ok && items.front().updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

            if (ok) {
                for (size_t i = 0; i < items.front().constantBufferRequirements.size(); ++i) {
                    const auto &req = items.front().constantBufferRequirements[i];
                    ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;

        ok = items.front().batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        std::vector<void*> mappedPtrs;
        mappedPtrs.reserve(items.front().instanceBufferRequirements.size());
        std::vector<StructuredBufferResource*> mappedBuffers;
        mappedBuffers.reserve(items.front().instanceBufferRequirements.size());

        for (const auto &req : items.front().instanceBufferRequirements) {
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
            if (!items[i].submitInstanceFunction(instanceMaps, shaderVariableBinder, i)) {
                ok = false;
                break;
            }
        }

        for (auto *b : mappedBuffers) {
            if (b) b->Unmap();
        }

        if (!ok) continue;

        auto renderCommandOpt = items.front().renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses2DStandard() {
    if (renderPasses2DStandard_.empty()) return;

    for (const auto &passInfo : renderPasses2DStandard_) {
        if (!passInfo.window ||
            passInfo.window->IsPendingDestroy() || passInfo.window->IsMinimized() || !passInfo.window->IsVisible()) {
            continue;
        }
        if (!passInfo.batchedRenderFunction || !passInfo.renderCommandFunction) {
            continue;
        }

        const HWND hwnd = passInfo.window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;
        pipelineBinder.UsePipeline(passInfo.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo.pipelineName);
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo.constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo.constantBufferRequirements.size());

            for (const auto &req : passInfo.constantBufferRequirements) {
                ConstantBufferKey cbKey{ hwnd, passInfo.pipelineName, 0, req.shaderNameKey, req.byteSize };
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
            if (passInfo.updateConstantBuffersFunction) {
                ok = ok && passInfo.updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

            if (ok) {
                for (const auto &req : passInfo.constantBufferRequirements) {
                    ConstantBufferKey cbKey{ hwnd, passInfo.pipelineName, 0, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;

        ok = passInfo.batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        auto renderCommandOpt = passInfo.renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = 1;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses3DInstancing() {
    if (renderPasses3DInstancing_.empty()) return;

    for (auto &kv : renderPasses3DInstancing_) {
        const auto &key = kv.first;
        auto &items = kv.second;
        if (items.empty()) continue;

        Window *window = items.front().window;
        if (!window ||
            window->IsPendingDestroy() || window->IsMinimized() || !window->IsVisible()) {
            continue;
        }

        auto it = windowBinders_.find(key.hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;
        pipelineBinder.UsePipeline(key.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, key.pipelineName);
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, key.hwnd);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = static_cast<std::uint32_t>(items.size());
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(items.front().constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(items.front().constantBufferRequirements.size());

            for (const auto &req : items.front().constantBufferRequirements) {
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
            if (items.front().updateConstantBuffersFunction) {
                ok = ok && items.front().updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

            if (ok) {
                for (size_t i = 0; i < items.front().constantBufferRequirements.size(); ++i) {
                    const auto &req = items.front().constantBufferRequirements[i];
                    ConstantBufferKey cbKey{ key.hwnd, key.pipelineName, key.key, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;

        ok = items.front().batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        std::vector<void*> mappedPtrs;
        mappedPtrs.reserve(items.front().instanceBufferRequirements.size());
        std::vector<StructuredBufferResource*> mappedBuffers;
        mappedBuffers.reserve(items.front().instanceBufferRequirements.size());

        for (const auto &req : items.front().instanceBufferRequirements) {
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
            if (!items[i].submitInstanceFunction(instanceMaps, shaderVariableBinder, i)) {
                ok = false;
                break;
            }
        }

        for (auto *b : mappedBuffers) {
            if (b) b->Unmap();
        }

        if (!ok) continue;

        auto renderCommandOpt = items.front().renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = instanceCount;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
    }
}

void Renderer::RenderPasses3DStandard() {
    if (renderPasses3DStandard_.empty()) return;

    for (const auto &passInfo : renderPasses3DStandard_) {
        if (!passInfo.window ||
            passInfo.window->IsPendingDestroy() || passInfo.window->IsMinimized() || !passInfo.window->IsVisible()) {
            continue;
        }
        if (!passInfo.batchedRenderFunction || !passInfo.renderCommandFunction) {
            continue;
        }

        const HWND hwnd = passInfo.window->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) continue;

        auto &pipelineBinder = it->second;
        pipelineBinder.UsePipeline(passInfo.pipelineName);

        auto &shaderVariableBinder = pipelineManager_->GetShaderVariableBinder({}, passInfo.pipelineName);
        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
        shaderVariableBinder.SetCommandList(commandList);

        const std::uint32_t instanceCount = 1;
        bool ok = true;

        {
            std::vector<void*> cbMappedPtrs;
            cbMappedPtrs.reserve(passInfo.constantBufferRequirements.size());
            std::vector<ConstantBufferResource*> cbMappedBuffers;
            cbMappedBuffers.reserve(passInfo.constantBufferRequirements.size());

            for (const auto &req : passInfo.constantBufferRequirements) {
                ConstantBufferKey cbKey{ hwnd, passInfo.pipelineName, 0, req.shaderNameKey, req.byteSize };
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
            if (passInfo.updateConstantBuffersFunction) {
                ok = ok && passInfo.updateConstantBuffersFunction(constantBufferMaps, instanceCount);
            }

            for (auto *b : cbMappedBuffers) {
                if (b) b->Unmap();
            }

            if (ok) {
                for (const auto &req : passInfo.constantBufferRequirements) {
                    ConstantBufferKey cbKey{ hwnd, passInfo.pipelineName, 0, req.shaderNameKey, req.byteSize };
                    auto itCB = constantBuffers_.find(cbKey);
                    if (itCB == constantBuffers_.end() || !itCB->second.buffer) { ok = false; break; }
                    ok = ok && shaderVariableBinder.Bind(req.shaderNameKey, itCB->second.buffer.get());
                    if (!ok) break;
                }
            }
        }

        if (!ok) continue;

        ok = passInfo.batchedRenderFunction(shaderVariableBinder, instanceCount);
        if (!ok) continue;

        auto renderCommandOpt = passInfo.renderCommandFunction(pipelineBinder);
        if (!renderCommandOpt) continue;
        RenderCommand cmd = *renderCommandOpt;
        cmd.instanceCount = 1;
        cmd.startInstanceLocation = 0;
        IssueRenderCommand(commandList, cmd);
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
    if (!pass.window) return;

    if (pass.dimension == RenderDimension::D2) {
        if (pass.renderType == RenderType::Standard) {
            renderPasses2DStandard_.emplace_back(pass);
            return;
        }
        BatchKey key{ pass.window->GetWindowHandle(), pass.pipelineName, pass.batchKey };
        renderPasses2DInstancing_[key].emplace_back(pass);
        return;
    }

    if (pass.dimension == RenderDimension::D3) {
        if (pass.renderType == RenderType::Standard) {
            renderPasses3DStandard_.emplace_back(pass);
            return;
        }
        BatchKey key{ pass.window->GetWindowHandle(), pass.pipelineName, pass.batchKey };
        renderPasses3DInstancing_[key].emplace_back(pass);
        return;
    }
}

} // namespace KashipanEngine