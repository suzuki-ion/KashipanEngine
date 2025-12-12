#include "Triangle2D.h"
#include "Objects/GameObjects/Components/2D/Material2D.h"

namespace KashipanEngine {

Triangle2D::Triangle2D(const std::string &name) : GameObject2DBase(name, sizeof(Vertex), sizeof(Index), 3, 3) {
    LogScope scope;
}

bool Triangle2D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }
    auto v = GetVertexSpan<Vertex>();
    if (v.size() < 3) return false;
    v[0] = Vertex(-0.5f, -0.5f, 0.0f, 1.0f);
    v[1] = Vertex(0.0f, 0.5f, 0.0f, 1.0f);
    v[2] = Vertex(0.5f, -0.5f, 0.0f, 1.0f);
    auto i = GetIndexSpan<Index>();
    if (i.size() < 3) return false;
    i[0] = 0;
    i[1] = 1;
    i[2] = 2;
    return true;
}

std::optional<RenderCommand> Triangle2D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
