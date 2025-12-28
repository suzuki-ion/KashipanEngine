#include "Ellipse.h"
#include <numbers>
#include <cmath>

namespace KashipanEngine {

Ellipse::Ellipse(size_t segmentCount)
    : Object2DBase("Ellipse", sizeof(Vertex), sizeof(Index), segmentCount, segmentCount * 3) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    const auto vc = GetVertexCount();
    if (vc < 3) return;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < vc || i.size() < static_cast<size_t>(vc) * 3) return;

    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    for (UINT idx = 0; idx < vc; ++idx) {
        const float t = static_cast<float>(idx) / static_cast<float>(vc);
        const float a = t * twoPi;
        const float x = std::cos(a) * 0.5f;
        const float y = std::sin(a) * 0.5f;
        v[idx].position = Vector4(x, y, 0.0f, 1.0f);
        v[idx].texcoord = Vector2((x + 0.5f), (0.5f - y));
    }

    // triangle fan indices (0, k, k+1)
    for (UINT k = 1; k < vc - 1; ++k) {
        const size_t base = static_cast<size_t>(k - 1) * 3;
        i[base + 0] = 0;
        i[base + 1] = static_cast<Index>(k);
        i[base + 2] = static_cast<Index>(k + 1);
    }
    // last triangle
    {
        const size_t base = static_cast<size_t>(vc - 2) * 3;
        i[base + 0] = 0;
        i[base + 1] = static_cast<Index>(vc - 1);
        i[base + 2] = 1;
    }
}

bool Ellipse::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Ellipse::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
