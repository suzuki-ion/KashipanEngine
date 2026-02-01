#include "Renderer.h"
#include "Core/Window.h"
#include "Core/DirectXCommon.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/ScreenBuffer.h"
#include "Graphics/ShadowMapBuffer.h"
#include "Graphics/PostEffectComponents/IPostEffectComponent.h"
#include "Utilities/EntityComponentSystem/WorldECS.h"
#include "ECS/EcsComponents.h"
#include "Assets/TextureManager.h"
#include "Assets/SamplerManager.h"
#include <unordered_map>
#include <chrono>
#include <cstring>
#include <algorithm>

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

namespace KashipanEngine {

namespace {
using Clock = std::chrono::high_resolution_clock;

struct LightCounts {
    std::uint32_t pointCount = 0;
    std::uint32_t spotCount = 0;
    std::uint32_t padding[2] = {0, 0};
};

struct TransformInstanceData {
    Matrix4x4 world{};
};
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

void Renderer::RenderFrame(Passkey<GraphicsEngine>, WorldECS &world) {
    if (!directXCommon_ || !pipelineManager_) {
        return;
    }

    for (auto &[hwnd, binder] : windowBinders_) {
        binder.Invalidate();
    }

    const auto &cameraEntities = world.GetEntitiesWithComponents<TransformComponent, CameraComponent>();
    CameraComponent *activeCamera = nullptr;
    TransformComponent *cameraTransform = nullptr;

    for (const auto &entity : cameraEntities) {
        auto *camera = world.GetComponent<CameraComponent>(entity);
        if (camera && camera->isActive) {
            activeCamera = camera;
            cameraTransform = world.GetComponent<TransformComponent>(entity);
            break;
        }
    }

    if (!activeCamera && !cameraEntities.empty()) {
        const auto entity = cameraEntities.front();
        activeCamera = world.GetComponent<CameraComponent>(entity);
        cameraTransform = world.GetComponent<TransformComponent>(entity);
    }

    if (!activeCamera) {
        return;
    }

    if (cameraTransform) {
        Vector3 forward = Vector3{0.0f, 0.0f, 1.0f};
        Matrix4x4 rot = Matrix4x4::Identity();
        rot.MakeRotate(cameraTransform->rotate);
        forward = forward.Transform(rot);

        Vector3 up{0.0f, 1.0f, 0.0f};
        Vector3 eye = cameraTransform->translate;
        Vector3 target = eye + forward;

        activeCamera->buffer.view.MakeViewMatrix(eye, target, up);
        activeCamera->buffer.eyePosition = Vector4{eye.x, eye.y, eye.z, 1.0f};
        activeCamera->buffer.fov = activeCamera->fovY;

        if (activeCamera->type == CameraComponent::CameraType::Perspective) {
            activeCamera->buffer.projection.MakePerspectiveFovMatrix(activeCamera->fovY, activeCamera->aspectRatio, activeCamera->nearClip, activeCamera->farClip);
        } else {
            activeCamera->buffer.projection.MakeOrthographicMatrix(activeCamera->orthoLeft, activeCamera->orthoTop, activeCamera->orthoRight, activeCamera->orthoBottom, activeCamera->nearClip, activeCamera->farClip);
        }

        activeCamera->buffer.viewProjection = activeCamera->buffer.view * activeCamera->buffer.projection;
    }

    if (!ecsCameraBuffer_) {
        ecsCameraBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(CameraComponent::CameraBuffer));
    }

    {
        void *map = ecsCameraBuffer_->Map();
        if (map) {
            std::memcpy(map, &activeCamera->buffer, sizeof(CameraComponent::CameraBuffer));
        }
    }

    if (!ecsDirectionalLightBuffer_) {
        ecsDirectionalLightBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(DirectionalLightComponent));
    }

    if (!ecsLightCountsBuffer_) {
        ecsLightCountsBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(LightCounts));
    }

    std::vector<PointLightComponent> pointLights;
    const auto &pointLightEntities = world.GetEntitiesWithComponents<PointLightComponent>();
    pointLights.reserve(pointLightEntities.size());
    for (const auto &entity : pointLightEntities) {
        auto *light = world.GetComponent<PointLightComponent>(entity);
        if (light) {
            pointLights.push_back(*light);
        }
    }

    std::vector<SpotLightComponent> spotLights;
    const auto &spotLightEntities = world.GetEntitiesWithComponents<SpotLightComponent>();
    spotLights.reserve(spotLightEntities.size());
    for (const auto &entity : spotLightEntities) {
        auto *light = world.GetComponent<SpotLightComponent>(entity);
        if (light) {
            spotLights.push_back(*light);
        }
    }

    const size_t pointCount = pointLights.size();
    const size_t spotCount = spotLights.size();

    const size_t pointBufferCount = std::max<size_t>(1, pointCount);
    const size_t spotBufferCount = std::max<size_t>(1, spotCount);

    if (!ecsPointLightBuffer_ || ecsPointLightBuffer_->GetElementStride() != sizeof(PointLightComponent)
        || ecsPointLightBuffer_->GetElementCount() < pointBufferCount) {
        ecsPointLightBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(PointLightComponent), pointBufferCount);
    }

    if (!ecsSpotLightBuffer_ || ecsSpotLightBuffer_->GetElementStride() != sizeof(SpotLightComponent)
        || ecsSpotLightBuffer_->GetElementCount() < spotBufferCount) {
        ecsSpotLightBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(SpotLightComponent), spotBufferCount);
    }

    LightCounts lightCounts{};
    lightCounts.pointCount = static_cast<std::uint32_t>(pointCount);
    lightCounts.spotCount = static_cast<std::uint32_t>(spotCount);

    {
        void *map = ecsLightCountsBuffer_->Map();
        if (map) {
            std::memcpy(map, &lightCounts, sizeof(LightCounts));
        }
    }

    DirectionalLightComponent directionalLight{};
    directionalLight.enabled = 0;
    const auto &dirLightEntities = world.GetEntitiesWithComponents<DirectionalLightComponent>();
    if (!dirLightEntities.empty()) {
        auto *light = world.GetComponent<DirectionalLightComponent>(dirLightEntities.front());
        if (light) {
            directionalLight = *light;
        }
    }

    {
        void *map = ecsDirectionalLightBuffer_->Map();
        if (map) {
            std::memcpy(map, &directionalLight, sizeof(DirectionalLightComponent));
        }
    }

    {
        void *map = ecsPointLightBuffer_->Map();
        if (map) {
            std::memset(map, 0, sizeof(PointLightComponent) * pointBufferCount);
            if (pointCount > 0) {
                std::memcpy(map, pointLights.data(), sizeof(PointLightComponent) * pointCount);
            }
        }
    }

    {
        void *map = ecsSpotLightBuffer_->Map();
        if (map) {
            std::memset(map, 0, sizeof(SpotLightComponent) * spotBufferCount);
            if (spotCount > 0) {
                std::memcpy(map, spotLights.data(), sizeof(SpotLightComponent) * spotCount);
            }
        }
    }

    if (!ecsTransformBuffer_) {
        ecsTransformBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(TransformInstanceData), 1);
    }

    if (!ecsMaterialBuffer_) {
        ecsMaterialBuffer_ = std::make_unique<StructuredBufferResource>(sizeof(MaterialComponent::InstanceData), 1);
    }

    auto updateTransformWorld = [&](Entity entity, auto &selfRef) -> void {
        auto *transform = world.GetComponent<TransformComponent>(entity);
        if (!transform || !transform->dirty) return;

        Matrix4x4 local = Matrix4x4::Identity();
        local.MakeAffine(transform->scale, transform->rotate, transform->translate);

        if (transform->parent != Entity(-1)) {
            selfRef(transform->parent, selfRef);
            auto *parent = world.GetComponent<TransformComponent>(transform->parent);
            if (parent) {
                transform->world = local * parent->world;
            } else {
                transform->world = local;
            }
        } else {
            transform->world = local;
        }

        transform->dirty = false;
        ++transform->version;
    };

    const auto &renderEntities = world.GetEntitiesWithComponents<TransformComponent, MeshComponent, MaterialComponent, RenderPipelineComponent>();
    for (const auto &entity : renderEntities) {
        auto *transform = world.GetComponent<TransformComponent>(entity);
        auto *mesh = world.GetComponent<MeshComponent>(entity);
        auto *material = world.GetComponent<MaterialComponent>(entity);
        auto *pipeline = world.GetComponent<RenderPipelineComponent>(entity);
        if (!transform || !mesh || !material || !pipeline) {
            continue;
        }

        Window *targetWindow = nullptr;
        if (world.HasComponent<RenderTargetComponent>(entity)) {
            auto *target = world.GetComponent<RenderTargetComponent>(entity);
            if (target) {
                targetWindow = target->window;
            }
        }

        if (!targetWindow && !windowBinders_.empty()) {
            targetWindow = Window::GetWindow(windowBinders_.begin()->first);
        }

        if (!targetWindow || targetWindow->IsPendingDestroy() || targetWindow->IsMinimized() || !targetWindow->IsVisible()) {
            continue;
        }

        const HWND hwnd = targetWindow->GetWindowHandle();
        auto it = windowBinders_.find(hwnd);
        if (it == windowBinders_.end()) {
            continue;
        }

        auto *commandList = directXCommon_->GetRecordedCommandList(Passkey<Renderer>{}, hwnd);
        if (!commandList) {
            continue;
        }

        updateTransformWorld(entity, updateTransformWorld);

        PipelineBinder pipelineBinder = it->second;
        pipelineBinder.SetCommandList(commandList);
        pipelineBinder.UsePipeline(pipeline->pipelineName);

        auto &shaderBinder = pipelineManager_->GetShaderVariableBinder({}, pipeline->pipelineName);
        shaderBinder.SetCommandList(commandList);

        if (shaderBinder.GetNameMap().Contains("Vertex:gCamera")) {
            shaderBinder.Bind("Vertex:gCamera", ecsCameraBuffer_.get());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gCamera")) {
            shaderBinder.Bind("Pixel:gCamera", ecsCameraBuffer_.get());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gDirectionalLight")) {
            shaderBinder.Bind("Pixel:gDirectionalLight", ecsDirectionalLightBuffer_.get());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:LightCounts")) {
            shaderBinder.Bind("Pixel:LightCounts", ecsLightCountsBuffer_.get());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gPointLights")) {
            shaderBinder.Bind("Pixel:gPointLights", ecsPointLightBuffer_->GetGPUDescriptorHandle());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gSpotLights")) {
            shaderBinder.Bind("Pixel:gSpotLights", ecsSpotLightBuffer_->GetGPUDescriptorHandle());
        }

        TransformInstanceData transformInstance{};
        transformInstance.world = transform->world;
        if (void *map = ecsTransformBuffer_->Map()) {
            std::memcpy(map, &transformInstance, sizeof(TransformInstanceData));
        }

        if (void *map = ecsMaterialBuffer_->Map()) {
            std::memcpy(map, &material->instanceData, sizeof(MaterialComponent::InstanceData));
        }

        if (shaderBinder.GetNameMap().Contains("Vertex:gTransformationMatrices")) {
            shaderBinder.Bind("Vertex:gTransformationMatrices", ecsTransformBuffer_->GetGPUDescriptorHandle());
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gMaterials")) {
            shaderBinder.Bind("Pixel:gMaterials", ecsMaterialBuffer_->GetGPUDescriptorHandle());
        }

        if (shaderBinder.GetNameMap().Contains("Pixel:gTexture") && material->textureHandle != TextureManager::kInvalidHandle) {
            TextureManager::BindTexture(&shaderBinder, "Pixel:gTexture", material->textureHandle);
        }
        if (shaderBinder.GetNameMap().Contains("Pixel:gSampler") && material->samplerHandle != SamplerManager::kInvalidHandle) {
            SamplerManager::BindSampler(&shaderBinder, "Pixel:gSampler", material->samplerHandle);
        }

        if (mesh->vertexBuffer && mesh->vertexCount > 0) {
            if (mesh->vertexView.SizeInBytes == 0 && mesh->vertexStride > 0) {
                mesh->vertexView = mesh->vertexBuffer->GetView(mesh->vertexStride);
            }
            pipelineBinder.SetVertexBufferView(0, 1, &mesh->vertexView);
        }

        if (mesh->indexBuffer && mesh->indexCount > 0) {
            if (mesh->indexView.SizeInBytes == 0) {
                mesh->indexView = mesh->indexBuffer->GetView();
            }
            pipelineBinder.SetIndexBufferView(&mesh->indexView);
            commandList->DrawIndexedInstanced(mesh->indexCount, 1, 0, 0, 0);
        } else if (mesh->vertexCount > 0) {
            commandList->DrawInstanced(mesh->vertexCount, 1, 0, 0);
        }
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

    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? shadowMapSys2DStandard_ : shadowMap2DStandard_).push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            if (isSystem) shadowMapSys2DInstancing_[key].push_back(p);
            else shadowMap2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? shadowMapSys3DStandard_ : shadowMap3DStandard_).push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            if (isSystem) shadowMapSys3DInstancing_[key].push_back(p);
            else shadowMap3DInstancing_[key].push_back(p);
        }
    }

    return handle;
}

bool Renderer::UnregisterPersistentShadowMapRenderPass(PersistentShadowMapPassHandle handle) {
    if (!handle) return false;
    auto it = persistentShadowMapPassesById_.find(handle.id);
    if (it == persistentShadowMapPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;
    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    const void *targetKey = static_cast<const void*>(p->shadowMapBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? shadowMapSys2DStandard_ : shadowMap2DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto &map = isSystem ? shadowMapSys2DInstancing_ : shadowMap2DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? shadowMapSys3DStandard_ : shadowMap3DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto &map = isSystem ? shadowMapSys3DInstancing_ : shadowMap3DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
            }
        }
    }

    persistentShadowMapPassesById_.erase(it);
    return true;
}

void Renderer::RenderScreenPasses() {
    if (persistentScreenPasses_.empty()) return;
    if (!directXCommon_) return;

    for (const auto *passInfo : persistentScreenPasses_) {
        if (!passInfo || !passInfo->screenBuffer) continue;

        auto *buffer = passInfo->screenBuffer;

        const auto &effectPasses = buffer->BuildPostEffectPasses(Passkey<Renderer>{});
        if (effectPasses.empty()) {
            continue;
        }

        const void *targetKey = static_cast<const void *>(buffer);

        for (const auto &fx : effectPasses) {
            if (fx.pipelineName.empty()) {
                break;
            }

            ID3D12GraphicsCommandList *commandList = nullptr;
            bool useCustomRecord = static_cast<bool>(fx.beginRecordFunction);

            if (!useCustomRecord) {
                commandList = buffer->BeginRecord(Passkey<Renderer>{}, true);
                if (!commandList) {
                    break;
                }
            } else {
                commandList = buffer->GetRecordedCommandList(Passkey<Renderer>{});
                if (!commandList) {
                    break;
                }

                // カスタムBeginRecord処理（RTV/viewport等の設定もここで行う）
                if (!fx.beginRecordFunction(commandList)) {
                    break;
                }
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
                if (!useCustomRecord) {
                    buffer->EndRecord(Passkey<Renderer>{}, true);
                }
                break;
            }

            auto renderCommandOpt = fx.renderCommandFunction(pipelineBinder);
            if (!renderCommandOpt) {
                if (!useCustomRecord) {
                    buffer->EndRecord(Passkey<Renderer>{}, true);
                }
                break;
            }

            RenderCommand cmd;
            cmd.vertexCount = renderCommandOpt->vertexCount;
            cmd.indexCount = renderCommandOpt->indexCount;
            cmd.instanceCount = 1;
            cmd.startVertexLocation = renderCommandOpt->startVertexLocation;
            cmd.startIndexLocation = renderCommandOpt->baseVertexLocation;
            cmd.startInstanceLocation = 0;
            IssueRenderCommand(commandList, cmd);

            if (!useCustomRecord) {
                buffer->EndRecord(Passkey<Renderer>{}, false);
            } else {
                if (fx.endRecordFunction) {
                    fx.endRecordFunction(commandList);
                }
            }
        }
    }
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

            // フレーム開始で RecordState は作成済み。実際の BeginRecord は初回使用時に行う。
            if (!sb->IsRecording(Passkey<Renderer>{})) {
                if (!sb->BeginRecord(Passkey<Renderer>{}, false)) {
                    ScreenBuffer::MarkDiscard(Passkey<Renderer>{}, sb);
                    continue;
                }
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
    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? persistentSys2DStandard_ : persistent2DStandard_).push_back(p);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            if (isSystem) persistentSys2DInstancing_[key].push_back(p);
            else persistent2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? persistentSys3DStandard_ : persistent3DStandard_).push_back(p);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            if (isSystem) persistentSys3DInstancing_[key].push_back(p);
            else persistent3DInstancing_[key].push_back(p);
        }
    }
    return handle;
}

bool Renderer::UnregisterPersistentRenderPass(PersistentPassHandle handle) {
    if (!handle) return false;
    auto it = persistentPassesById_.find(handle.id);
    if (it == persistentPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;
    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? persistentSys2DStandard_ : persistent2DStandard_);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            auto &map = isSystem ? persistentSys2DInstancing_ : persistent2DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? persistentSys3DStandard_ : persistent3DStandard_);
        } else {
            BatchKey key{ static_cast<const void*>(p->window->GetWindowHandle()), p->pipelineName, p->batchKey };
            auto &map = isSystem ? persistentSys3DInstancing_ : persistent3DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
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
    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? offscreenSys2DStandard_ : offscreen2DStandard_).push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            if (isSystem) offscreenSys2DInstancing_[key].push_back(p);
            else offscreen2DInstancing_[key].push_back(p);
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            (isSystem ? offscreenSys3DStandard_ : offscreen3DStandard_).push_back(p);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            if (isSystem) offscreenSys3DInstancing_[key].push_back(p);
            else offscreen3DInstancing_[key].push_back(p);
        }
    }

    return handle;
}

bool Renderer::UnregisterPersistentOffscreenRenderPass(PersistentOffscreenPassHandle handle) {
    if (!handle) return false;
    auto it = persistentOffscreenPassesById_.find(handle.id);
    if (it == persistentOffscreenPassesById_.end()) return false;

    const RenderPass *p = &it->second.pass;
    const bool isSystem = (p->objectType == ObjectType::SystemObject);

    auto eraseFromVector = [&](std::vector<const RenderPass*> &v) {
        for (auto vit = v.begin(); vit != v.end(); ++vit) {
            if (*vit == p) { v.erase(vit); break; }
        }
    };

    const void *targetKey = static_cast<const void*>(p->screenBuffer);

    if (p->dimension == RenderDimension::D2) {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? offscreenSys2DStandard_ : offscreen2DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto &map = isSystem ? offscreenSys2DInstancing_ : offscreen2DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
            }
        }
    } else {
        if (p->renderType == RenderType::Standard) {
            eraseFromVector(isSystem ? offscreenSys3DStandard_ : offscreen3DStandard_);
        } else {
            BatchKey key{ targetKey, p->pipelineName, p->batchKey };
            auto &map = isSystem ? offscreenSys3DInstancing_ : offscreen3DInstancing_;
            auto itB = map.find(key);
            if (itB != map.end()) {
                auto &vec = itB->second;
                for (auto vit = vec.begin(); vit != vec.end(); ++vit) {
                    if (*vit == p) { vec.erase(vit); break; }
                }
                if (vec.empty()) map.erase(itB);
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

void Renderer::RenderShadowMapPasses() {
    if (shadowMap2DStandard_.empty() && shadowMap3DStandard_.empty() &&
        shadowMap2DInstancing_.empty() && shadowMap3DInstancing_.empty() &&
        shadowMapSys2DStandard_.empty() && shadowMapSys3DStandard_.empty() &&
        shadowMapSys2DInstancing_.empty() && shadowMapSys3DInstancing_.empty()) {
        return;
    }

    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->shadowMapBuffer) return nullptr;
        return const_cast<void *>(static_cast<const void *>(passInfo->shadowMapBuffer));
    };

    // SystemObject -> GameObject (3D then 2D)
    Render3DStandard(shadowMapSys3DStandard_, getTargetKey);
    Render3DInstancing(shadowMapSys3DInstancing_, getTargetKey);
    Render3DStandard(shadowMap3DStandard_, getTargetKey);
    Render3DInstancing(shadowMap3DInstancing_, getTargetKey);

    Render2DStandard(shadowMapSys2DStandard_, getTargetKey);
    Render2DInstancing(shadowMapSys2DInstancing_, getTargetKey);
    Render2DStandard(shadowMap2DStandard_, getTargetKey);
    Render2DInstancing(shadowMap2DInstancing_, getTargetKey);
}

void Renderer::RenderOffscreenPasses() {
    if (offscreen2DStandard_.empty() && offscreen3DStandard_.empty() &&
        offscreen2DInstancing_.empty() && offscreen3DInstancing_.empty() &&
        offscreenSys2DStandard_.empty() && offscreenSys3DStandard_.empty() &&
        offscreenSys2DInstancing_.empty() && offscreenSys3DInstancing_.empty()) {
        return;
    }

    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->screenBuffer) return nullptr;
        return const_cast<void *>(static_cast<const void *>(passInfo->screenBuffer));
    };

    // SystemObject -> GameObject (3D then 2D)
    Render3DStandard(offscreenSys3DStandard_, getTargetKey);
    Render3DInstancing(offscreenSys3DInstancing_, getTargetKey);
    Render3DStandard(offscreen3DStandard_, getTargetKey);
    Render3DInstancing(offscreen3DInstancing_, getTargetKey);

    Render2DStandard(offscreenSys2DStandard_, getTargetKey);
    //Render2DInstancing(offscreenSys2DInstancing_, getTargetKey);
    Render2DStandard(offscreen2DStandard_, getTargetKey);
    //Render2DInstancing(offscreen2DInstancing_, getTargetKey);
}

void Renderer::RenderPersistentPasses() {
    auto getTargetKey = [](const RenderPass *passInfo) -> void * {
        if (!passInfo || !passInfo->window) return nullptr;
        const HWND hwnd = passInfo->window->GetWindowHandle();
        return const_cast<void *>(static_cast<const void *>(hwnd));
    };

    // SystemObject -> GameObject (3D then 2D)
    Render3DStandard(persistentSys3DStandard_, getTargetKey);
    Render3DInstancing(persistentSys3DInstancing_, getTargetKey);
    Render3DStandard(persistent3DStandard_, getTargetKey);
    Render3DInstancing(persistent3DInstancing_, getTargetKey);

    Render2DStandard(persistentSys2DStandard_, getTargetKey);
    Render2DInstancing(persistentSys2DInstancing_, getTargetKey);
    Render2DStandard(persistent2DStandard_, getTargetKey);
    Render2DInstancing(persistent2DInstancing_, getTargetKey);
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