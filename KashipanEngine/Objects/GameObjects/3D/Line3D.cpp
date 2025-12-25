#include "Line3D.h"

namespace KashipanEngine {

Line3D::Line3D(size_t lineCount, const std::string &name)
    : Object3DBase(name, sizeof(Vertex), sizeof(Index), lineCount + 1, lineCount + 1) {
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
        v[idx].normal = Vector3(0.0f, 1.0f, 0.0f);
        i[idx] = static_cast<Index>(idx);
    }
}

bool Line3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Line3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() < 2) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
