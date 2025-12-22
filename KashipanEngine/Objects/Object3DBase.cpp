#include "Objects/Object3DBase.h"
#include <algorithm>
#include "Objects/ObjectContext.h"
#include "Objects/Components/3D/Transform3D.h"

namespace KashipanEngine {

Object3DBase::~Object3DBase() {
    // 終了処理の呼び出し
    for (auto &c : components_) {
        c->Finalize();
    }
    // コンポーネントの破棄
    components3D_.clear();
    components3DIndexByName_.clear();
    components_.clear();
}

RenderPassInfo3D Object3DBase::CreateRenderPass(Window *targetWindow, const std::string &pipelineName, const std::string &passName) {
    RenderPassInfo3D passInfo;
    passInfo.window = targetWindow;
    passInfo.pipelineName = pipelineName;
    passInfo.passName = passName;
    passInfo.renderFunction = [this](ShaderVariableBinder &shaderBinder) -> bool {
        auto failures = BindShaderVariablesToComponents(shaderBinder);
        if (!failures.empty()) {
            for (const auto &f : failures) {
                Log(Translation("engine.object3d.shader.binding.failed")
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
    if (!name.empty()) name_ = name;
    context_ = std::make_unique<Object3DContext>(Passkey<Object3DBase>{}, this);
    RegisterComponent<Transform3D>();
}

Object3DBase::Object3DBase(const std::string &name, size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    if (!name.empty()) name_ = name;
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
    RegisterComponent<Transform3D>();
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

void Object3DBase::Render() {
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
    for (size_t i = 0; i < components3D_.size(); ++i) {
        components3DIndexByName_.emplace(components3D_[i]->GetComponentType(), i);
    }

    return true;
}

} // namespace KashipanEngine
