#include "Objects/Object2DBase.h"
#include <algorithm>
#include <cstring>
#include <random>
#include "Objects/ObjectContext.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
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

Object2DBase::~Object2DBase() {
    DetachFromRenderer();
    // コンポーネントの終了処理
    for (auto &c : components_) {
        c->Finalize();
    }
    // コンポーネントの破棄
    components2D_.clear();
    components2DIndexByName_.clear();
    components2DIndexByType_.clear();
    components_.clear();
}

Object2DBase::RenderPassRegistrationHandle Object2DBase::GenerateRegistrationHandle() const {
    return RenderPassRegistrationHandle{ MakeRandomNonZeroU64() };
}

Object2DBase::RenderPassRegistrationHandle Object2DBase::AttachToRenderer(Window *targetWindow, const std::string &pipelineName) {
    if (!targetWindow) return {};
    auto *renderer = Window::GetRenderer(Passkey<Object2DBase>{});
    if (!renderer) return {};

    RenderPassRegistrationEntry entry{};
    entry.info.targetKind = RenderTargetKind::Window;
    entry.info.window = targetWindow;
    entry.info.screenBuffer = nullptr;
    entry.info.pipelineName = pipelineName;

    // unique handle
    RenderPassRegistrationHandle h{};
    do {
        h = GenerateRegistrationHandle();
    } while (!h || renderPassRegistrations_.find(h.value) != renderPassRegistrations_.end());

    entry.info.handle = h;

    auto pass = CreateRenderPass(targetWindow, pipelineName);
    entry.windowHandle = renderer->RegisterPersistentRenderPass(std::move(pass));
    if (!entry.windowHandle) {
        return {};
    }

    renderPassRegistrations_.emplace(h.value, std::move(entry));
    return h;
}

Object2DBase::RenderPassRegistrationHandle Object2DBase::AttachToRenderer(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    if (!targetBuffer) return {};
    auto *renderer = Window::GetRenderer(Passkey<Object2DBase>{});
    if (!renderer) return {};

    RenderPassRegistrationEntry entry{};
    entry.info.targetKind = RenderTargetKind::ScreenBuffer;
    entry.info.window = nullptr;
    entry.info.screenBuffer = targetBuffer;
    entry.info.pipelineName = pipelineName;

    RenderPassRegistrationHandle h{};
    do {
        h = GenerateRegistrationHandle();
    } while (!h || renderPassRegistrations_.find(h.value) != renderPassRegistrations_.end());

    entry.info.handle = h;

    auto pass = CreateRenderPass(targetBuffer, pipelineName);
    entry.offscreenHandle = renderer->RegisterPersistentOffscreenRenderPass(std::move(pass));
    if (!entry.offscreenHandle) {
        return {};
    }

    renderPassRegistrations_.emplace(h.value, std::move(entry));
    return h;
}

void Object2DBase::DetachFromRenderer() {
    auto *renderer = Window::GetRenderer(Passkey<Object2DBase>{});
    if (renderer) {
        for (auto &kv : renderPassRegistrations_) {
            auto &e = kv.second;
            if (e.windowHandle) {
                renderer->UnregisterPersistentRenderPass(e.windowHandle);
            }
            if (e.offscreenHandle) {
                renderer->UnregisterPersistentOffscreenRenderPass(e.offscreenHandle);
            }
        }
    }
    renderPassRegistrations_.clear();
}

bool Object2DBase::DetachFromRenderer(RenderPassRegistrationHandle handle) {
    if (!handle) return false;

    auto it = renderPassRegistrations_.find(handle.value);
    if (it == renderPassRegistrations_.end()) return false;

    auto *renderer = Window::GetRenderer(Passkey<Object2DBase>{});
    if (renderer) {
        if (it->second.windowHandle) {
            renderer->UnregisterPersistentRenderPass(it->second.windowHandle);
        }
        if (it->second.offscreenHandle) {
            renderer->UnregisterPersistentOffscreenRenderPass(it->second.offscreenHandle);
        }
    }

    renderPassRegistrations_.erase(it);
    return true;
}

std::vector<Object2DBase::RenderPassRegistrationInfo> Object2DBase::GetRenderPassRegistrations() const {
    std::vector<RenderPassRegistrationInfo> out;
    out.reserve(renderPassRegistrations_.size());
    for (const auto &kv : renderPassRegistrations_) {
        out.push_back(kv.second.info);
    }
    return out;
}

std::optional<Object2DBase::RenderPassRegistrationInfo> Object2DBase::GetRenderPassRegistration(RenderPassRegistrationHandle handle) const {
    if (!handle) return std::nullopt;
    auto it = renderPassRegistrations_.find(handle.value);
    if (it == renderPassRegistrations_.end()) return std::nullopt;
    return it->second.info;
}

void Object2DBase::SetUniqueBatchKey() {
    instanceBatchKey_ = std::hash<std::string>{}(name_) ^ (std::hash<const void *>{}(this) << 1);
    renderType_ = RenderType::Standard;
}

RenderPass Object2DBase::CreateRenderPass(Window *targetWindow, const std::string &pipelineName) {
    if (!cachedRenderPass_) {
        RenderPass passInfo(Passkey<Object2DBase>{});

        passInfo.passName = passName_;
        passInfo.renderType = renderType_;
        passInfo.constantBufferRequirements = constantBufferRequirements_;
        passInfo.updateConstantBuffersFunction = updateConstantBuffersFunction_;

        passInfo.batchKey = instanceBatchKey_;
        passInfo.instanceBufferRequirements = {
            {"Vertex:gTransformationMatrices", sizeof(Transform2D::InstanceData)},
            {"Pixel:gMaterials", sizeof(Material2D::InstanceData)},
        };
        passInfo.submitInstanceFunction = [this](void *instanceMaps, ShaderVariableBinder &shaderBinder, std::uint32_t instanceIndex) -> bool {
            return SubmitInstance(instanceMaps, shaderBinder, instanceIndex);
        };
        passInfo.batchedRenderFunction = [this](ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount) -> bool {
            return RenderBatched(shaderBinder, instanceCount);
        };
        passInfo.renderCommandFunction = [this](PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
            return CreateRenderCommand(pipelineBinder);
        };

        cachedRenderPass_.emplace(std::move(passInfo));
    }

    // Update only variable parts
    cachedRenderPass_->window = targetWindow;
    cachedRenderPass_->screenBuffer = nullptr;
    cachedRenderPass_->pipelineName = pipelineName;
    cachedRenderPass_->renderType = renderType_;
    cachedRenderPass_->constantBufferRequirements = constantBufferRequirements_;
    cachedRenderPass_->updateConstantBuffersFunction = updateConstantBuffersFunction_;
    cachedRenderPass_->batchKey = instanceBatchKey_;

    return *cachedRenderPass_;
}

RenderPass Object2DBase::CreateRenderPass(ScreenBuffer *targetBuffer, const std::string &pipelineName) {
    if (!cachedRenderPass_) {
        // Initialize common/static parts once
        RenderPass passInfo(Passkey<Object2DBase>{});

        passInfo.passName = passName_;
        passInfo.renderType = renderType_;
        passInfo.constantBufferRequirements = constantBufferRequirements_;
        passInfo.updateConstantBuffersFunction = updateConstantBuffersFunction_;

        passInfo.batchKey = instanceBatchKey_;
        passInfo.instanceBufferRequirements = {
            {"Vertex:gTransformationMatrices", sizeof(Transform2D::InstanceData)},
            {"Pixel:gMaterials", sizeof(Material2D::InstanceData)},
        };
        passInfo.submitInstanceFunction = [this](void *instanceMaps, ShaderVariableBinder &shaderBinder, std::uint32_t instanceIndex) -> bool {
            return SubmitInstance(instanceMaps, shaderBinder, instanceIndex);
        };
        passInfo.batchedRenderFunction = [this](ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount) -> bool {
            return RenderBatched(shaderBinder, instanceCount);
        };
        passInfo.renderCommandFunction = [this](PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
            return CreateRenderCommand(pipelineBinder);
        };

        cachedRenderPass_.emplace(std::move(passInfo));
    }

    cachedRenderPass_->window = nullptr;
    cachedRenderPass_->screenBuffer = targetBuffer;
    cachedRenderPass_->pipelineName = pipelineName;
    cachedRenderPass_->renderType = renderType_;
    cachedRenderPass_->constantBufferRequirements = constantBufferRequirements_;
    cachedRenderPass_->updateConstantBuffersFunction = updateConstantBuffersFunction_;
    cachedRenderPass_->batchKey = instanceBatchKey_;

    return *cachedRenderPass_;
}

bool Object2DBase::RenderBatched(ShaderVariableBinder &shaderBinder, std::uint32_t instanceCount) {
    if (!Render(shaderBinder)) return false;

    auto failures = BindShaderVariablesToComponents(shaderBinder);
    if (!failures.empty()) return false;

    if (instanceCount == 0) return false;

    return true;
}

bool Object2DBase::SubmitInstance(void *instanceMaps, ShaderVariableBinder &shaderBinder, std::uint32_t instanceIndex) {
    if (!instanceMaps) return false;
    auto **maps = static_cast<void **>(instanceMaps);

    if (shaderBinder.GetNameMap().Contains("Vertex:gTransformationMatrices")) {
        void *transformMap = maps[0];
        if (transform_) {
            auto r = transform_->SubmitInstance(transformMap, instanceIndex);
            if (r != std::nullopt && r.value() == false) return false;
        }
    }
    if (shaderBinder.GetNameMap().Contains("Pixel:gMaterials")) {
        void *materialMap = maps[1];
        if (material_) {
            auto r = material_->SubmitInstance(materialMap, instanceIndex);
            if (r != std::nullopt && r.value() == false) return false;
        }
    }

    return true;
}

bool Object2DBase::RegisterComponent(std::unique_ptr<IObjectComponent> comp) {
    if (!comp) return false;

    auto *comp2D = dynamic_cast<IObjectComponent2D *>(comp.get());
    if (comp2D == nullptr) return false;
    
    // 登録上限を超えていないか確認
    size_t maxCount = comp->GetMaxComponentCountPerObject();
    size_t existingCount = HasComponents2D(comp->GetComponentType());
    if (existingCount >= maxCount) return false;
    if (context_) comp->SetOwnerContext(static_cast<IObjectContext*>(context_.get()));

    // 登録処理
    const std::string key = comp->GetComponentType();

    components_.push_back(std::move(comp));
    const size_t ownerIdx = components_.size() - 1;
    componentsIndexByName_.emplace(key, ownerIdx);

    components2D_.push_back(comp2D);
    const size_t idx2D = components2D_.size() - 1;
    components2DIndexByName_.emplace(key, idx2D);
    components2DIndexByType_.emplace(std::type_index(typeid(*comp2D)), idx2D);

    // シェーダーバインド予定のコンポーネントかを記録
    if (components_.back()->BindShaderVariables(nullptr) != std::nullopt) {
        shaderBindingComponentIndices_.push_back(ownerIdx);
    }

    // 初期化処理の呼び出し
    components_.back()->Initialize();
    return true;
}

Object2DBase::Object2DBase(const std::string &name) {
    LogScope scope;
    if (!name.empty()) name_ = name;
    passName_ = name_;
    // 頂点やインデックスが要らないタイプのオブジェクトは描画用オブジェクトでないことが多いので、
    // インスタンスバッチキーにはポインタ値も混ぜる
    instanceBatchKey_ = std::hash<std::string>{}(name_) ^ (std::hash<const void *>{}(this) << 1);
    context_ = std::make_unique<Object2DContext>(Passkey<Object2DBase>{}, this);
    RegisterComponent<Transform2D>();
    transform_ = GetComponent2D<Transform2D>();
}

Object2DBase::Object2DBase(const std::string &name, size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    if (!name.empty()) name_ = name;
    passName_ = name_;
    instanceBatchKey_ = std::hash<std::string>{}(name_);
    if (vertexCount == 0 && indexCount == 0) {
        Log(Translation("engine.object2d.invalid.vertex.index.count")
            + "Name: " + name
            + ", VertexCount: " + std::to_string(vertexCount)
            + ", IndexCount: " + std::to_string(indexCount), LogSeverity::Critical);
        throw std::invalid_argument("Object2DBase requires vertex or index count");
    }
    vertexCount_ = static_cast<UINT>(vertexCount);
    indexCount_ = static_cast<UINT>(indexCount);
    // 頂点バッファの初期化 (vertexByteSize は 1頂点のサイズ)
    if (vertexCount_ > 0) {
        size_t totalVertexBytes = vertexByteSize * vertexCount_;
        vertexBuffer_ = std::make_unique<VertexBufferResource>(totalVertexBytes, initialVertexData);
        vertexBufferView_ = vertexBuffer_->GetView(static_cast<UINT>(vertexByteSize));
        vertexData_ = vertexBuffer_->Map();
    }
    // インデックスバッファの初期化 (indexByteSize は 1インデックスあたりのサイズ)
    if (indexCount_ > 0) {
        size_t totalIndexBytes = indexByteSize * indexCount_;
        indexBuffer_ = std::make_unique<IndexBufferResource>(totalIndexBytes, DXGI_FORMAT_R32_UINT, initialIndexData);
        indexBufferView_ = indexBuffer_->GetView();
        indexData_ = indexBuffer_->Map();
    }
    // 永続コンテキストの生成（2D）
    context_ = std::make_unique<Object2DContext>(Passkey<Object2DBase>{}, this);

    // 基本コンポーネントの登録
    RegisterComponent<Transform2D>();

    Vector4 defaultMaterialColor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureManager::TextureHandle defaultTexture
        = TextureManager::GetTextureFromFileName("white1x1.png");
    SamplerManager::SamplerHandle defaultSampler = 1;
    RegisterComponent<Material2D>(defaultMaterialColor, defaultTexture, defaultSampler);

    transform_ = GetComponent2D<Transform2D>();
    material_ = GetComponent2D<Material2D>();
}

std::optional<RenderCommand> Object2DBase::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (vertexCount_ == 0 && indexCount_ == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    auto cmd = CreateDefaultRenderCommand();
    if (cmd.vertexCount == 0 && cmd.indexCount == 0) return std::nullopt;
    return cmd;
}

RenderCommand Object2DBase::CreateDefaultRenderCommand() const {
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

void Object2DBase::Update() {
    for (auto *c : components2D_) {
        c->Update();
    }
    OnUpdate();
}

void Object2DBase::Render() {
    /*std::vector<IObjectComponent*> sorted;
    sorted.reserve(components_.size());
    for (auto &c : components_) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](const IObjectComponent* a, const IObjectComponent* b) {
        return a->GetRenderPriority() < b->GetRenderPriority();
    });*/
}

std::vector<Object2DBase::ShaderBindingFailureInfo> Object2DBase::BindShaderVariablesToComponents(ShaderVariableBinder &shaderBinder) {
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

bool Object2DBase::RemoveComponent2D(const std::string &componentName, size_t index) {
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

    // 終了処理
    components_[ownerRemoveIdx]->Finalize();

    // 2D配列から削除（同一ポインタを探す）
    if (auto *c2d = dynamic_cast<IObjectComponent2D *>(components_[ownerRemoveIdx].get())) {
        for (size_t i = 0; i < components2D_.size(); ++i) {
            if (components2D_[i] == c2d) {
                components2D_.erase(components2D_.begin() + static_cast<std::ptrdiff_t>(i));
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

    components2DIndexByName_.clear();
    components2DIndexByType_.clear();
    for (size_t i = 0; i < components2D_.size(); ++i) {
        components2DIndexByName_.emplace(components2D_[i]->GetComponentType(), i);
        components2DIndexByType_.emplace(std::type_index(typeid(*components2D_[i])), i);
    }

    return true;
}

#if defined(USE_IMGUI)
void Object2DBase::ShowImGui() {
    ImGui::TextUnformatted(Translation("engine.imgui.object2d.name").c_str());
    ImGui::SameLine();
    ImGui::TextUnformatted(name_.c_str());

    if (ImGui::CollapsingHeader(Translation("engine.imgui.object2d.components").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto &c : components_) {
            if (!c) continue;
            if (ImGui::TreeNode(c->GetComponentType().c_str())) {
                c->ShowImGui();
                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader(Translation("engine.imgui.object2d.derived").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ShowImGuiDerived();
    }
}
#endif

} // namespace KashipanEngine