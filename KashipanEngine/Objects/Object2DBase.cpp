#include "Objects/Object2DBase.h"
#include <algorithm>
#include "Objects/ObjectContext.h"
#include "Objects/Components/2D/Transform2D.h"

namespace KashipanEngine {

Object2DBase::~Object2DBase() {
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

RenderPassInfo2D Object2DBase::CreateRenderPass(Window *targetWindow, const std::string &pipelineName, const std::string &passName) {
    RenderPassInfo2D passInfo;
    passInfo.window = targetWindow;
    passInfo.pipelineName = pipelineName;
    passInfo.passName = passName;
    passInfo.renderFunction = [this](ShaderVariableBinder &shaderBinder) -> bool {
        auto failures = BindShaderVariablesToComponents(shaderBinder);
        if (!failures.empty()) {
            for (const auto &f : failures) {
                Log(Translation("engine.object2d.shader.binding.failed")
                    + " ComponentType: " + f.componentType
                    + ", ComponentIndex: " + std::to_string(f.componentIndex)
                    , LogSeverity::Warning);
            }
            return false;
        }
        return Render(shaderBinder);
    };
    passInfo.renderCommandFunction = [this](PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
        return CreateRenderCommand(pipelineBinder);
    };
    return passInfo;
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
    context_ = std::make_unique<Object2DContext>(Passkey<Object2DBase>{}, this);
    RegisterComponent<Transform2D>();
}

Object2DBase::Object2DBase(const std::string &name, size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    if (!name.empty()) name_ = name;
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
    // インデックスバッファの初期化 (indexByteSize は 1インデックスのサイズ)
    if (indexCount_ > 0) {
        size_t totalIndexBytes = indexByteSize * indexCount_;
        indexBuffer_ = std::make_unique<IndexBufferResource>(totalIndexBytes, DXGI_FORMAT_R32_UINT, initialIndexData);
        indexBufferView_ = indexBuffer_->GetView();
        indexData_ = indexBuffer_->Map();
    }
    // 永続コンテキストの生成（2D）
    context_ = std::make_unique<Object2DContext>(Passkey<Object2DBase>{}, this);
    RegisterComponent<Transform2D>();
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
    std::vector<IObjectComponent*> sorted;
    sorted.reserve(components_.size());
    for (auto &c : components_) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](const IObjectComponent* a, const IObjectComponent* b) {
        return a->GetUpdatePriority() < b->GetUpdatePriority();
    });

    for (auto *c : sorted) {
        c->Update();
    }
    OnUpdate();
}

void Object2DBase::Render() {
    std::vector<IObjectComponent*> sorted;
    sorted.reserve(components_.size());
    for (auto &c : components_) sorted.push_back(c.get());
    std::stable_sort(sorted.begin(), sorted.end(), [](const IObjectComponent* a, const IObjectComponent* b) {
        return a->GetRenderPriority() < b->GetRenderPriority();
    });

    for (auto *c : sorted) {
        c->Render();
    }
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

} // namespace KashipanEngine