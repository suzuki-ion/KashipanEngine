#include "Ring3D.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace KashipanEngine {

Ring3D::Ring3D(size_t segmentCount, float innerRadius, float outerRadius, float startAngle, float endAngle, UvMode uvMode)
    : EmptyObject("Ring3D", sizeof(Vertex), sizeof(Index), (segmentCount + 1) * 2, segmentCount * 6),
      segmentCount_(segmentCount),
      innerRadius_(innerRadius),
      outerRadius_(outerRadius),
      startAngle_(startAngle),
      endAngle_(endAngle),
      uvMode_(uvMode) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    UpdateVertices();
    GenerateIndices();
}

void Ring3D::SetInnerRadius(float innerRadius) {
    innerRadius_ = innerRadius;
    UpdateVertices();
}

void Ring3D::SetOuterRadius(float outerRadius) {
    outerRadius_ = outerRadius;
    UpdateVertices();
}

void Ring3D::SetStartAngle(float startAngle) {
    startAngle_ = startAngle;
    UpdateVertices();
}

void Ring3D::SetEndAngle(float endAngle) {
    endAngle_ = endAngle;
    UpdateVertices();
}

void Ring3D::SetUvMode(UvMode uvMode) {
    uvMode_ = uvMode;
    UpdateVertices();
}

void Ring3D::UpdateVertices() {
    const auto vc = GetVertexCount();
    if (vc < 4) return;

    auto v = GetVertexSpan<Vertex>();
    if (v.size() < vc) return;

    if (segmentCount_ == 0) return;

    const float inner = std::min(innerRadius_, outerRadius_);
    const float outer = std::max(innerRadius_, outerRadius_);
    const float angleRange = endAngle_ - startAngle_;
    const float denom = static_cast<float>(segmentCount_);
    const Vector3 normal(0.0f, 0.0f, 1.0f);

    for (UINT seg = 0; seg <= segmentCount_; ++seg) {
        const float t = denom > 0.0f ? static_cast<float>(seg) / denom : 0.0f;
        const float angle = startAngle_ + angleRange * t;
        const float c = std::cos(angle);
        const float s = std::sin(angle);

        const size_t innerIdx = static_cast<size_t>(seg) * 2;
        const size_t outerIdx = innerIdx + 1;

        const float ix = c * inner;
        const float iy = s * inner;
        const float ox = c * outer;
        const float oy = s * outer;

        v[innerIdx].position = Vector4(ix, iy, 0.0f, 1.0f);
        v[outerIdx].position = Vector4(ox, oy, 0.0f, 1.0f);

        if (uvMode_ == UvMode::Curved) {
            v[innerIdx].texcoord = Vector2(t, 0.0f);
            v[outerIdx].texcoord = Vector2(t, 1.0f);
        } else {
            v[innerIdx].texcoord = Vector2((ix / (outer * 2.0f)) + 0.5f, 0.5f - (iy / (outer * 2.0f)));
            v[outerIdx].texcoord = Vector2((ox / (outer * 2.0f)) + 0.5f, 0.5f - (oy / (outer * 2.0f)));
        }

        v[innerIdx].normal = normal;
        v[outerIdx].normal = normal;
    }
}

void Ring3D::GenerateIndices() {
    const auto vc = GetVertexCount();
    const auto ic = GetIndexCount();
    if (vc < 4 || ic == 0) return;

    auto i = GetIndexSpan<Index>();
    if (i.size() < ic) return;

    if (segmentCount_ == 0) return;

    size_t w = 0;
    for (UINT seg = 0; seg < segmentCount_; ++seg) {
        const Index i0 = static_cast<Index>(seg * 2);
        const Index i1 = static_cast<Index>(seg * 2 + 1);
        const Index i2 = static_cast<Index>((seg + 1) * 2);
        const Index i3 = static_cast<Index>((seg + 1) * 2 + 1);

        i[w++] = i0;
        i[w++] = i3;
        i[w++] = i1;

        i[w++] = i0;
        i[w++] = i2;
        i[w++] = i3;
    }
}

bool Ring3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Ring3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
