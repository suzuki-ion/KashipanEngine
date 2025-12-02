#include "Line2D.h"

namespace KashipanEngine {

bool Line2D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    return false;
}

std::optional<RenderCommand> Line2D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
