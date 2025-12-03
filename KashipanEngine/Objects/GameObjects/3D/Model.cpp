#include "Model.h"

namespace KashipanEngine {

bool Model::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    // TODO: 実装
    return false;
}

std::optional<RenderCommand> Model::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
