#include "Triangle3D.h"

namespace KashipanEngine {

Triangle3D::Triangle3D(const std::string &name)
    : Object3DBase(name, sizeof(Vertex), sizeof(Index), 3, 3) {
    LogScope scope;
}

bool Triangle3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0) {
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

std::optional<RenderCommand> Triangle3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
