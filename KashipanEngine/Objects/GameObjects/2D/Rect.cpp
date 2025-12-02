#include "Rect.h"

namespace KashipanEngine {

bool Rect::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    auto v = GetVertexSpan<Vertex>();
    if (v.size() < 4) return false;
    v[0] = Vertex(-0.5f, -0.5f, 0.0f, 1.0f);
    v[1] = Vertex(-0.5f, 0.5f, 0.0f, 1.0f);
    v[2] = Vertex(0.5f, 0.5f, 0.0f, 1.0f);
    v[3] = Vertex(0.5f, -0.5f, 0.0f, 1.0f);
    auto i = GetIndexSpan<Index>();
    if (i.size() < 6) return false;
    i[0] = 0;
    i[1] = 1;
    i[2] = 2;
    i[3] = 0;
    i[4] = 2;
    i[5] = 3;
    return true;
}

std::optional<RenderCommand> Rect::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
