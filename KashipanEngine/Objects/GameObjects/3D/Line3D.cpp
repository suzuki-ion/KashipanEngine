#include "Line3D.h"

namespace KashipanEngine {

bool Line3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    return false;
}

std::optional<RenderCommand> Line3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() < 2) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
