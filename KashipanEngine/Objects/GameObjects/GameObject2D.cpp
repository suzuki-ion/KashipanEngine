#include "GameObject2D.h"

namespace KashipanEngine {

GameObject2D::GameObject2D(size_t vertexCount, size_t indexCount)
    : vertexCount_(static_cast<UINT>(vertexCount)), indexCount_(static_cast<UINT>(indexCount)) {
    LogScope scope;
    if (vertexCount == 0 && indexCount == 0) {
        Log(Translation("engine.gameobject2d.invalid.vertex.index.count") + "VertexCount: " + std::to_string(vertexCount) + ", IndexCount: " + std::to_string(indexCount), LogSeverity::Critical);
        throw std::invalid_argument("GameObject2D requires vertex or index count");
    }
    // 頂点バッファの初期化
    if (vertexCount_ > 0) {
        vertexBuffer_ = std::make_unique<VertexBufferResource>(sizeof(Vector4) * vertexCount_);
        // テスト用の頂点データ（必要数がある場合のみ設定）
        vertexData_ = static_cast<Vector4 *>(vertexBuffer_->Map());
        if (vertexData_) {
            if (vertexCount_ >= 3) {
                vertexData_[0] = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
                vertexData_[1] = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
                vertexData_[2] = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
            }
        }
        vertexBuffer_->Unmap();
        vertexBufferView_ = vertexBuffer_->GetView(sizeof(Vector4));
    }
    // インデックスバッファの初期化
    if (indexCount_ > 0) {
        indexBuffer_ = std::make_unique<IndexBufferResource>(sizeof(uint32_t) * indexCount_, DXGI_FORMAT_R32_UINT);
        // テスト用のインデックスデータ（必要数がある場合のみ設定）
        uint32_t *indexData = static_cast<uint32_t *>(indexBuffer_->Map());
        if (indexData) {
            if (indexCount_ >= 3) {
                indexData[0] = 0;
                indexData[1] = 1;
                indexData[2] = 2;
            }
        }
        indexBuffer_->Unmap();
        indexBufferView_ = indexBuffer_->GetView();
    }
}

RenderPassInfo2D GameObject2D::CreateRenderPass(Window *targetWindow, const std::string &pipelineName, const std::string &passName) {
    RenderPassInfo2D passInfo{ Passkey<GameObject2D>() };
    passInfo.window = targetWindow;
    passInfo.pipelineName = pipelineName;
    passInfo.passName = passName;
    passInfo.renderFunction = [this](ShaderVariableBinder &shaderBinder, PipelineBinder &pipelineBinder) -> std::optional<RenderCommand> {
        return Draw(shaderBinder, pipelineBinder);
    };
    return passInfo;
}

std::optional<RenderCommand> GameObject2D::Draw([[maybe_unused]] ShaderVariableBinder &shaderBinder, [[maybe_unused]] PipelineBinder &pipelineBinder) {
    if (!vertexBuffer_) {
        return std::nullopt;
    }
    pipelineBinder.SetVertexBuffer(vertexBuffer_.get(), sizeof(Vector4), 0);
    RenderCommand cmd{};
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