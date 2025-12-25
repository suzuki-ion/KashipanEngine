#include "Triangle2D.h"

namespace KashipanEngine {

Triangle2D::Triangle2D(const std::string &name) : Object2DBase(name, sizeof(Vertex), sizeof(Index), 3, 3) {
    LogScope scope;

    auto v = GetVertexSpan<Vertex>();
    if (v.size() >= 3) {
        v[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
        v[1].position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
        v[2].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
        v[0].texcoord = Vector2(0.0f, 1.0f);
        v[1].texcoord = Vector2(0.5f, 0.0f);
        v[2].texcoord = Vector2(1.0f, 1.0f);
    }

    auto i = GetIndexSpan<Index>();
    if (i.size() >= 3) {
        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
    }
}

bool Triangle2D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Triangle2D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
