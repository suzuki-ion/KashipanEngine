#include "Box.h"

namespace KashipanEngine {

bool Box::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    return false;
}

std::optional<RenderCommand> Box::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
