#include "Rect.h"

namespace KashipanEngine {

Rect::Rect(const std::string &name) : Object2DBase(name, sizeof(Vertex), sizeof(Index), 4, 6) {
    LogScope scope;

    auto v = GetVertexSpan<Vertex>();
    if (v.size() >= 4) {
        v[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
        v[1].position = Vector4(-0.5f, 0.5f, 0.0f, 1.0f);
        v[2].position = Vector4(0.5f, 0.5f, 0.0f, 1.0f);
        v[3].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);

        v[0].texcoord = Vector2(0.0f, 1.0f);
        v[1].texcoord = Vector2(0.0f, 0.0f);
        v[2].texcoord = Vector2(1.0f, 0.0f);
        v[3].texcoord = Vector2(1.0f, 1.0f);
    }

    auto i = GetIndexSpan<Index>();
    if (i.size() >= 6) {
        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;
    }
}

bool Rect::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Rect::CreateRenderCommand(PipelineBinder &pipelineBinder) {

    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
