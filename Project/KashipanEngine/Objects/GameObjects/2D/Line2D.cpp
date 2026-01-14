#include "Line2D.h"

namespace KashipanEngine {

Line2D::Line2D(size_t lineCount)
    : Object2DBase("Line2D", sizeof(Vertex), sizeof(Index), lineCount + 1, lineCount + 1) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    const auto vc = GetVertexCount();
    if (vc < 2) return;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < vc || i.size() < vc) return;

    const float denom = static_cast<float>(vc - 1);
    for (UINT idx = 0; idx < vc; ++idx) {
        const float t = denom > 0.0f ? static_cast<float>(idx) / denom : 0.0f;
        v[idx].position = Vector4(-0.5f + t, 0.0f, 0.0f, 1.0f);
        v[idx].texcoord = Vector2(t, 0.0f);
        i[idx] = static_cast<Index>(idx);
    }
}

bool Line2D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Line2D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
