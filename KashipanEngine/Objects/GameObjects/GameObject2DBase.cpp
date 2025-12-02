#include "GameObject2DBase.h"

namespace KashipanEngine {

RenderPassInfo2D GameObject2DBase::CreateRenderPass(Window *targetWindow, const std::string &pipelineName, const std::string &passName) {
    RenderPassInfo2D passInfo;
    passInfo.window = targetWindow;
    passInfo.pipelineName = pipelineName;
    passInfo.passName = passName;
    passInfo.renderFunction = [this](ShaderVariableBinder &shaderBinder) -> bool {
        return Render(shaderBinder);
    };
    passInfo.renderCommandFunction = [this](PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
        return CreateRenderCommand(pipelineBinder);
    };
    return passInfo;
}

GameObject2DBase::GameObject2DBase(size_t vertexByteSize, size_t indexByteSize, size_t vertexCount, size_t indexCount, void *initialVertexData, void *initialIndexData) {
    LogScope scope;
    if (vertexCount == 0 && indexCount == 0) {
        Log(Translation("engine.gameobject2d.invalid.vertex.index.count") + "VertexCount: " + std::to_string(vertexCount) + ", IndexCount: " + std::to_string(indexCount), LogSeverity::Critical);
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

} // namespace KashipanEngine