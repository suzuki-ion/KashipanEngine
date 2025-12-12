#include "GameObject2DBase.h"
#include "GameObjectContext.h"

namespace KashipanEngine {

GameObject2DBase::~GameObject2DBase() {
    // コンポーネントの終了処理
    for (auto &c : components_) {
        c->Finalize();
    }
    // コンポーネントの破棄
    components_.clear();
}

RenderPassInfo2D GameObject2DBase::CreateRenderPass(Window *targetWindow, const std::string &pipelineName, const std::string &passName) {
    RenderPassInfo2D passInfo;
    passInfo.window = targetWindow;
    passInfo.pipelineName = pipelineName;
    passInfo.passName = passName;
    passInfo.renderFunction = [this](ShaderVariableBinder &shaderBinder) -> bool {
        auto failures = BindShaderVariablesToComponents(shaderBinder);
        if (!failures.empty()) {
            for (const auto &f : failures) {
                Log(Translation("engine.gameobject2d.shader.binding.failed")
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

bool GameObject2DBase::RegisterComponent(std::unique_ptr<IGameObjectComponent> comp) {
    if (!comp) return false;
    if (dynamic_cast<IGameObjectComponent2D *>(comp.get()) == nullptr) return false;
    
    // 登録上限を超えていないか確認
    size_t maxCount = comp->GetMaxComponentCountPerObject();
    size_t existingCount = HasComponents2D(comp->GetComponentType());
    if (existingCount >= maxCount) return false;
    if (context_) comp->SetOwnerContext(context_.get());

    // 登録処理
    const std::string key = comp->GetComponentType();
    components_.push_back(std::move(comp));
    const size_t idx = components_.size() - 1;
    componentsIndexByName_.emplace(key, idx);

    // シェーダーバインド予定のコンポーネントかを記録
    if (components_.back()->BindShaderVariables(nullptr) != std::nullopt) {
        shaderBindingComponentIndices_.push_back(idx);
    }

    // 初期化処理の呼び出し
    components_.back()->Initialize();
    return true;
}

GameObject2DBase::GameObject2DBase(const std::string &name, size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    if (!name.empty()) name_ = name;
    if (vertexCount == 0 && indexCount == 0) {
        Log(Translation("engine.gameobject2d.invalid.vertex.index.count")
            + "Name: " + name
            + ", VertexCount: " + std::to_string(vertexCount)
            + ", IndexCount: " + std::to_string(indexCount), LogSeverity::Critical);
        throw std::invalid_argument("GameObject2DBase requires vertex or index count");
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
    context_ = std::make_unique<GameObject2DContext>(Passkey<GameObject2DBase>{}, this);
}

std::optional<RenderCommand> GameObject2DBase::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (vertexCount_ == 0 && indexCount_ == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    auto cmd = CreateDefaultRenderCommand();
    if (cmd.vertexCount == 0 && cmd.indexCount == 0) return std::nullopt;
    return cmd;
}

RenderCommand GameObject2DBase::CreateDefaultRenderCommand() const {
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

void GameObject2DBase::Update() {
    for (auto &c : components_) {
        c->PreUpdate();
    }
    OnUpdate();
    for (auto &c : components_) {
        c->PostUpdate();
    }
}

void GameObject2DBase::PreRender() {
    for (auto &c : components_) {
        c->PreRender();
    }
}

std::vector<GameObject2DBase::ShaderBindingFailureInfo> GameObject2DBase::BindShaderVariablesToComponents(ShaderVariableBinder &shaderBinder) {
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

} // namespace KashipanEngine