#include "Objects/Object3DBase.h"
#include <algorithm>
#include <cstring>
#include <random>
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Graphics/ShadowMapBuffer.h"
#include <string>

namespace KashipanEngine {

namespace {
std::uint64_t MakeRandomNonZeroU64() {
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    std::uint64_t v = 0;
    do {
        v = rng();
    } while (v == 0);
    return v;
}
}

Object3DBase::~Object3DBase() {
    DetachFromRenderer();
    // 終了処理の呼び出し
    for (auto &c : components_) {
        c->Finalize();
    }
    // コンポーネントの破棄
    components3D_.clear();
    components3DIndexByName_.clear();
    components3DIndexByType_.clear();
    components_.clear();
}

Object3DBase::RenderPassRegistrationHandle Object3DBase::GenerateRegistrationHandle() const {
    return RenderPassRegistrationHandle{ MakeRandomNonZeroU64() };
}

void Object3DBase::SetRenderer(Passkey<GameEngine>, Renderer* renderer) {
    sRenderer = renderer;
}

Object3DBase::RenderPassRegistrationHandle Object3DBase::AttachToRenderer(Window *targetWindow, const std::string &pipelineName,
    std::optional<std::vector<RenderPass::ConstantBufferRequirement>> constantBufferRequirements,
    std::optional<std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)>> updateConstantBuffersFunction,
    std::optional<std::vector<RenderPass::InstanceBufferRequirement>> instanceBufferRequirements,
    std::optional<std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)>> submitInstanceFunction) {
    if (!targetWindow) return {};
    auto *renderer = sRenderer;
    if (!renderer) return {};

    RenderPassRegistrationEntry entry{};
    entry.info.targetKind = RenderTargetKind::Window;
    entry.info.window = targetWindow;
    entry.info.screenBuffer = nullptr;
    entry.info.pipelineName = pipelineName;

    RenderPassRegistrationHandle h{};
    do {
        h = GenerateRegistrationHandle();
    } while (!h || renderPassRegistrations_.find(h.value) != renderPassRegistrations_.end());

    entry.info.handle = h;

    auto pass = CreateRenderPass(pipelineName);
    pass.window = targetWindow;

    if (constantBufferRequirements.has_value()) {
        pass.constantBufferRequirements = std::move(*constantBufferRequirements);
    } else if (constantBufferRequirements_.has_value()) {
        pass.constantBufferRequirements = *constantBufferRequirements_;
    }

    if (updateConstantBuffersFunction.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction;
    } else if (updateConstantBuffersFunction_.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction_;
    }

    if (instanceBufferRequirements.has_value()) {
        pass.instanceBufferRequirements = std::move(*instanceBufferRequirements);
    } else if (instanceBufferRequirements_.has_value()) {
        pass.instanceBufferRequirements = *instanceBufferRequirements_;
    }

    if (submitInstanceFunction.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction;
    } else if (submitInstanceFunction_.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction_;
    }

    entry.windowHandle = renderer->RegisterPersistentRenderPass(std::move(pass));
    if (!entry.windowHandle) {
        return {};
    }

    renderPassRegistrations_.emplace(h.value, std::move(entry));
    return h;
}

Object3DBase::RenderPassRegistrationHandle Object3DBase::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName,
    std::optional<std::vector<RenderPass::ConstantBufferRequirement>> constantBufferRequirements,
    std::optional<std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)>> updateConstantBuffersFunction,
    std::optional<std::vector<RenderPass::InstanceBufferRequirement>> instanceBufferRequirements,
    std::optional<std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)>> submitInstanceFunction) {
    if (!targetBuffer) return {};
    auto *renderer = sRenderer;
    if (!renderer) return {};

    RenderPassRegistrationEntry entry{};
    entry.info.targetKind = RenderTargetKind::ScreenBuffer;
    entry.info.window = nullptr;
    entry.info.screenBuffer = targetBuffer;
    entry.info.shadowMapBuffer = nullptr;
    entry.info.pipelineName = pipelineName;

    RenderPassRegistrationHandle h{};
    do {
        h = GenerateRegistrationHandle();
    } while (!h || renderPassRegistrations_.find(h.value) != renderPassRegistrations_.end());

    entry.info.handle = h;

    auto pass = CreateRenderPass(pipelineName);
    pass.screenBuffer = targetBuffer;

    if (constantBufferRequirements.has_value()) {
        pass.constantBufferRequirements = std::move(*constantBufferRequirements);
    } else if (constantBufferRequirements_.has_value()) {
        pass.constantBufferRequirements = *constantBufferRequirements_;
    }

    if (updateConstantBuffersFunction.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction;
    } else if (updateConstantBuffersFunction_.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction_;
    }

    if (instanceBufferRequirements.has_value()) {
        pass.instanceBufferRequirements = std::move(*instanceBufferRequirements);
    } else if (instanceBufferRequirements_.has_value()) {
        pass.instanceBufferRequirements = *instanceBufferRequirements_;
    }

    if (submitInstanceFunction.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction;
    } else if (submitInstanceFunction_.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction_;
    }

    entry.offscreenHandle = renderer->RegisterPersistentOffscreenRenderPass(std::move(pass));
    if (!entry.offscreenHandle) {
        return {};
    }

    renderPassRegistrations_.emplace(h.value, std::move(entry));
    return h;
}

Object3DBase::RenderPassRegistrationHandle Object3DBase::AttachToRenderer(ShadowMapBuffer *targetBuffer, const std::string &pipelineName,
    std::optional<std::vector<RenderPass::ConstantBufferRequirement>> constantBufferRequirements,
    std::optional<std::function<bool(void *constantBufferMaps, std::uint32_t instanceCount)>> updateConstantBuffersFunction,
    std::optional<std::vector<RenderPass::InstanceBufferRequirement>> instanceBufferRequirements,
    std::optional<std::function<bool(void *instanceMaps, ShaderVariableBinder &, std::uint32_t instanceIndex)>> submitInstanceFunction) {
    if (!targetBuffer) return {};
    auto *renderer = sRenderer;
    if (!renderer) return {};

    RenderPassRegistrationEntry entry{};
    entry.info.targetKind = RenderTargetKind::ShadowMapBuffer;
    entry.info.window = nullptr;
    entry.info.screenBuffer = nullptr;
    entry.info.shadowMapBuffer = targetBuffer;
    entry.info.pipelineName = pipelineName;

    RenderPassRegistrationHandle h{};
    do {
        h = GenerateRegistrationHandle();
    } while (!h || renderPassRegistrations_.find(h.value) != renderPassRegistrations_.end());

    entry.info.handle = h;

    auto pass = CreateRenderPass(pipelineName);
    pass.shadowMapBuffer = targetBuffer;

    if (constantBufferRequirements.has_value()) {
        pass.constantBufferRequirements = std::move(*constantBufferRequirements);
    } else if (constantBufferRequirements_.has_value()) {
        pass.constantBufferRequirements = *constantBufferRequirements_;
    }

    if (updateConstantBuffersFunction.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction;
    } else if (updateConstantBuffersFunction_.has_value()) {
        pass.updateConstantBuffersFunction = *updateConstantBuffersFunction_;
    }

    if (instanceBufferRequirements.has_value()) {
        pass.instanceBufferRequirements = std::move(*instanceBufferRequirements);
    } else if (instanceBufferRequirements_.has_value()) {
        pass.instanceBufferRequirements = *instanceBufferRequirements_;
    }

    if (submitInstanceFunction.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction;
    } else if (submitInstanceFunction_.has_value()) {
        pass.submitInstanceFunction = *submitInstanceFunction_;
    }

    entry.shadowMapHandle = renderer->RegisterPersistentShadowMapRenderPass(std::move(pass));
    if (!entry.shadowMapHandle) {
        return {};
    }

    renderPassRegistrations_.emplace(h.value, std::move(entry));
    return h;
}

void Object3DBase::DetachFromRenderer() {
    auto *renderer = sRenderer;
    if (renderer) {
        for (auto &kv : renderPassRegistrations_) {
            auto &e = kv.second;
            if (e.windowHandle) {
                renderer->UnregisterPersistentRenderPass(e.windowHandle);
            }
            if (e.offscreenHandle) {
                renderer->UnregisterPersistentOffscreenRenderPass(e.offscreenHandle);
            }
            if (e.shadowMapHandle) {
                renderer->UnregisterPersistentShadowMapRenderPass(e.shadowMapHandle);
            }
        }
    }
    renderPassRegistrations_.clear();
}

bool Object3DBase::DetachFromRenderer(RenderPassRegistrationHandle handle) {
    if (!handle) return false;

    auto it = renderPassRegistrations_.find(handle.value);
    if (it == renderPassRegistrations_.end()) return false;

    auto *renderer = sRenderer;
    if (renderer) {
        if (it->second.windowHandle) {
            renderer->UnregisterPersistentRenderPass(it->second.windowHandle);
        }
        if (it->second.offscreenHandle) {
            renderer->UnregisterPersistentOffscreenRenderPass(it->second.offscreenHandle);
        }
        if (it->second.shadowMapHandle) {
            renderer->UnregisterPersistentShadowMapRenderPass(it->second.shadowMapHandle);
        }
    }

    renderPassRegistrations_.erase(it);
    return true;
}

std::vector<Object3DBase::RenderPassRegistrationInfo> Object3DBase::GetRenderPassRegistrations() const {
    std::vector<RenderPassRegistrationInfo> out;
    out.reserve(renderPassRegistrations_.size());
    for (const auto &kv : renderPassRegistrations_) {
        out.push_back(kv.second.info);
    }
    return out;
}

std::optional<Object3DBase::RenderPassRegistrationInfo> Object3DBase::GetRenderPassRegistration(RenderPassRegistrationHandle handle) const {
    if (!handle) return std::nullopt;
    auto it = renderPassRegistrations_.find(handle.value);
    if (it == renderPassRegistrations_.end()) return std::nullopt;
    return it->second.info;
}

void Object3DBase::SetUniqueBatchKey() {
    instanceBatchKey_ = std::hash<std::string>{}(name_) ^ (std::hash<const void *>{}(this) << 1);
    renderType_ = RenderType::Standard;
}

RenderPass Object3DBase::CreateRenderPass(const std::string &pipelineName) {
    if (!cachedRenderPass_) {
        RenderPass passInfo(Passkey<Object3DBase>{});

        passInfo.passName = passName_;
        passInfo.renderType = renderType_;
        passInfo.objectType = objectType_;

        passInfo.batchKey = instanceBatchKey_;
        passInfo.batchedRenderFunction = [this](ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount) -> bool {
            return RenderBatched(shaderBinder, instanceCount);
            };
        passInfo.renderCommandFunction = [this](PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
            return CreateRenderCommand(pipelineBinder);
            };

        cachedRenderPass_.emplace(std::move(passInfo));
    }

    cachedRenderPass_->instanceBufferRequirements = {
        {"Vertex:gTransformationMatrices", sizeof(Transform3D::InstanceData)},
        {"Pixel:gMaterials", sizeof(Material3D::InstanceData)},
    };
    cachedRenderPass_->submitInstanceFunction = [this](void *instanceMaps, ShaderVariableBinder & /*shaderBinder*/, std::uint32_t instanceIndex) -> bool {
        if (!instanceMaps) return false;
        auto **maps = static_cast<void **>(instanceMaps);

        if (transform_) {
            void *transformMap = maps[0];
            transform_->SubmitInstance(transformMap, instanceIndex);
        }
        if (material_) {
            void *materialMap = maps[1];
            material_->SubmitInstance(materialMap, instanceIndex);
        }
        return true;
    };

    cachedRenderPass_->pipelineName = pipelineName;
    cachedRenderPass_->renderType = renderType_;
    cachedRenderPass_->objectType = objectType_;
    cachedRenderPass_->batchKey = instanceBatchKey_;

    return *cachedRenderPass_;
}

bool Object3DBase::RenderBatched(ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount) {
    if (!Render(shaderBinder)) return false;

    auto failures = BindShaderVariablesToComponents(shaderBinder);
    if (!failures.empty()) return false;

    if (instanceCount == 0) return false;

    return true;
}

bool Object3DBase::SubmitInstance(void *instanceMaps, ShaderVariableBinder &shaderBinder, std::uint32_t instanceIndex) {
    (void)shaderBinder;
    if (!instanceMaps) return false;
    auto **maps = static_cast<void **>(instanceMaps);

    if (transform_) {
        void *transformMap = maps[0];
        transform_->SubmitInstance(transformMap, instanceIndex);
    }
    if (material_ && shaderBinder.GetNameMap().Contains("Pixel:gMaterials")) {
        void *materialMap = maps[1];
        material_->SubmitInstance(materialMap, instanceIndex);
    }

    return true;
}

bool Object3DBase::RegisterComponent(std::unique_ptr<IObjectComponent> comp) {
    if (!comp) return false;

    auto *comp3D = dynamic_cast<IObjectComponent3D *>(comp.get());
    if (comp3D == nullptr) return false;

    // 登録上限を超えていないか確認
    size_t maxCount = comp->GetMaxComponentCountPerObject();
    size_t existingCount = HasComponents3D(comp->GetComponentType());
    if (existingCount >= maxCount) return false;
    if (context_) comp->SetOwnerContext(static_cast<IObjectContext*>(context_.get()));
    
    // 登録処理
    const std::string key = comp->GetComponentType();

    components_.push_back(std::move(comp));
    const size_t ownerIdx = components_.size() - 1;
    componentsIndexByName_.emplace(key, ownerIdx);

    components3D_.push_back(comp3D);
    const size_t idx3D = components3D_.size() - 1;
    components3DIndexByName_.emplace(key, idx3D);
    components3DIndexByType_.emplace(std::type_index(typeid(*comp3D)), idx3D);

    // シェーダーバインド予定のコンポーネントかを記録
    if (components_.back()->BindShaderVariables(nullptr) != std::nullopt) {
        shaderBindingComponentIndices_.push_back(ownerIdx);
    }

    // 初期化処理の呼び出し
    components_.back()->Initialize();
    return true;
}

Object3DBase::Object3DBase(const std::string &name) {
    LogScope scope;
    objectType_ = ObjectType::SystemObject;
    if (!name.empty()) name_ = name;
    passName_ = name_;
    instanceBatchKey_ = std::hash<std::string>{}(name_);
    context_ = std::make_unique<Object3DContext>(Passkey<Object3DBase>{}, this);
    RegisterComponent<Transform3D>();
    transform_ = GetComponent3D<Transform3D>();
}

Object3DBase::Object3DBase(const std::string &name, size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    objectType_ = ObjectType::GameObject;
    if (!name.empty()) name_ = name;
    passName_ = name_;
    instanceBatchKey_ = std::hash<std::string>{}(name_);
    if (vertexCount == 0 && indexCount == 0) {
        Log(Translation("engine.object3d.invalid.vertex.index.count")
            + "Name: " + name
            + ", VertexCount: " + std::to_string(vertexCount)
            + ", IndexCount: " + std::to_string(indexCount), LogSeverity::Critical);
        throw std::invalid_argument("Object3DBase requires vertex or index count");
    }
    vertexCount_ = static_cast<UINT>(vertexCount);
    indexCount_ = static_cast<UINT>(indexCount);
    // 頂点バッファの初期化 (vertexByteSize は 1頂 vertexのサイズ)
    if (vertexCount_ > 0) {
        size_t totalVertexBytes = vertexByteSize * vertexCount_;
        vertexBuffer_ = std::make_unique<VertexBufferResource>(totalVertexBytes, initialVertexData);
        vertexBufferView_ = vertexBuffer_->GetView(static_cast<UINT>(vertexByteSize));
        vertexData_ = vertexBuffer_->Map();
    }
    // インデックスバッファの初期化 (indexByteSize は 1インデックスのサイズ)
    if (indexCount_ > 0) {
        size_t totalIndexBytes = indexByteSize * indexCount_;
        indexBuffer_ = std::make_unique<IndexBufferResource>(totalIndexBytes, DXGI_FORMAT_R32_UINT, initialIndexData);
        indexBufferView_ = indexBuffer_->GetView();
        indexData_ = indexBuffer_->Map();
    }
    // 永続コンテキストの生成（3D）
    context_ = std::make_unique<Object3DContext>(Passkey<Object3DBase>{}, this);

    // 基本コンポーネントの登録
    RegisterComponent<Transform3D>();

    Vector4 defaultMaterialColor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureManager::TextureHandle defaultTexture
        = TextureManager::GetTextureFromFileName("white1x1.png");
    SamplerManager::SamplerHandle defaultSampler = 1;
    RegisterComponent<Material3D>(defaultMaterialColor, defaultTexture, defaultSampler);

    transform_ = GetComponent3D<Transform3D>();
    material_ = GetComponent3D<Material3D>();
}

std::optional<RenderCommand> Object3DBase::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (vertexCount_ == 0 && indexCount_ == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    auto cmd = CreateDefaultRenderCommand();
    if (cmd.vertexCount == 0 && cmd.indexCount == 0) return std::nullopt;
    return cmd;
}

RenderCommand Object3DBase::CreateDefaultRenderCommand() const {
    RenderCommand cmd;
    cmd.vertexCount = vertexCount_;
    cmd.indexCount = indexCount_;
    cmd.instanceCount = 1;
    cmd.startVertexLocation = 0;
    cmd.startIndexLocation = 0;
    cmd.baseVertexLocation = 0;
    cmd.startInstanceLocation = 0;
    return cmd;
}

void Object3DBase::Update() {
    for (auto *c : components3D_) {
        c->Update();
    }
    OnUpdate();
}

void Object3DBase::Render() {
    /*std::vector<IObjectComponent*> sorted;
    sorted.reserve(components_.size());
    for (auto &c : components_) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](const IObjectComponent* a, const IObjectComponent* b) {
        return a->GetRenderPriority() < b->GetRenderPriority();
    });*/
}

std::vector<Object3DBase::ShaderBindingFailureInfo> Object3DBase::BindShaderVariablesToComponents(ShaderVariableBinder &shaderBinder) {
    std::vector<ShaderBindingFailureInfo> failures;
    for (size_t idx : shaderBindingComponentIndices_) {
        if (idx >= components_.size()) continue;
        auto &comp = components_[idx];
        auto result = comp->BindShaderVariables(&shaderBinder);
        if (result != std::nullopt && result.value() == false) {
            ShaderBindingFailureInfo info;
            info.componentIndex = idx;
            info.componentType = comp->GetComponentType();
            failures.push_back(info);
        }
    }
    return failures;
}

bool Object3DBase::RemoveComponent3D(const std::string &componentName, size_t index) {
    if (components_.empty()) return false;

    size_t matchCount = 0;
    size_t ownerRemoveIdx = static_cast<size_t>(-1);

    for (size_t i = 0; i < components_.size(); ++i) {
        const auto &c = components_[i];
        if (!c) continue;
        if (c->GetComponentType() != componentName) continue;
        if (matchCount == index) {
            ownerRemoveIdx = i;
            break;
        }
        ++matchCount;
    }

    if (ownerRemoveIdx == static_cast<size_t>(-1)) return false;

    components_[ownerRemoveIdx]->Finalize();

    // 3D配列から削除（同一ポインタを探す）
    if (auto *c3d = dynamic_cast<IObjectComponent3D *>(components_[ownerRemoveIdx].get())) {
        for (size_t i = 0; i < components3D_.size(); ++i) {
            if (components3D_[i] == c3d) {
                components3D_.erase(components3D_.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
    }

    // 所有配列から削除
    components_.erase(components_.begin() + static_cast<std::ptrdiff_t>(ownerRemoveIdx));

    // インデックス類を再構築
    componentsIndexByName_.clear();
    shaderBindingComponentIndices_.clear();
    for (size_t i = 0; i < components_.size(); ++i) {
        const std::string key = components_[i]->GetComponentType();
        componentsIndexByName_.emplace(key, i);
        if (components_[i]->BindShaderVariables(nullptr) != std::nullopt) {
            shaderBindingComponentIndices_.push_back(i);
        }
    }

    components3DIndexByName_.clear();
    components3DIndexByType_.clear();
    for (size_t i = 0; i < components3D_.size(); ++i) {
        components3DIndexByName_.emplace(components3D_[i]->GetComponentType(), i);
        components3DIndexByType_.emplace(std::type_index(typeid(*components3D_[i])), i);
    }

    return true;
}

#if defined(USE_IMGUI)
void Object3DBase::ShowImGui() {
    ImGui::TextUnformatted(Translation("engine.imgui.object3d.name").c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(name_.c_str());

    if (ImGui::CollapsingHeader(Translation("engine.imgui.object3d.components").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &c : components_) {
            if (!c) continue;
            if (ImGui::TreeNode(c->GetComponentType().c_str())) {
                c->ShowImGui();
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader(Translation("engine.imgui.object3d.derived").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ShowImGuiDerived();
    }
}
#endif

} // namespace KashipanEngine
