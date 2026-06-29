#include "Cylinder3D.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace KashipanEngine {

namespace {
constexpr float kMinHeight = 0.0001f;
constexpr float kMinRadius = 0.0001f;
}

Cylinder3D::Cylinder3D(size_t segmentCount, Caps caps, float height, float radius)
    : EmptyObject("Cylinder3D", sizeof(Vertex), sizeof(Index),
          (segmentCount + 1) * 2 + (segmentCount + 2) * 2,
          segmentCount * 6 + segmentCount * 3 * 2),
      segmentCount_(segmentCount),
      height_(height),
      radius_(radius),
      caps_(caps) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    UpdateGeometry();
}

void Cylinder3D::SetCaps(Caps caps) {
    caps_ = caps;
    UpdateGeometry();
}

void Cylinder3D::SetHeight(float height) {
    height_ = height;
    UpdateGeometry();
}

void Cylinder3D::SetRadius(float radius) {
    radius_ = radius;
    UpdateGeometry();
}

bool Cylinder3D::HasCap(Caps caps, Caps flag) {
    return (static_cast<int>(caps) & static_cast<int>(flag)) != 0;
}

void Cylinder3D::UpdateGeometry() {
    const auto vc = GetVertexCount();
    const auto ic = GetIndexCount();
    if (vc == 0 || ic == 0) return;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < vc || i.size() < ic) return;

    if (segmentCount_ < 3) return;

    const float height = std::max(height_, kMinHeight);
    const float radius = std::max(radius_, kMinRadius);
    const float halfHeight = height * 0.5f;
    const float twoPi = 2.0f * std::numbers::pi_v<float>;
    const float denom = static_cast<float>(segmentCount_);

    size_t vIndex = 0;
    size_t iIndex = 0;

    for (UINT seg = 0; seg <= segmentCount_; ++seg) {
        const float t = denom > 0.0f ? static_cast<float>(seg) / denom : 0.0f;
        const float angle = t * twoPi;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float x = c * radius;
        const float z = s * radius;

        const size_t bottomIdx = vIndex++;
        const size_t topIdx = vIndex++;

        v[bottomIdx].position = Vector4(x, -halfHeight, z, 1.0f);
        v[topIdx].position = Vector4(x, halfHeight, z, 1.0f);
        v[bottomIdx].texcoord = Vector2(t, 1.0f);
        v[topIdx].texcoord = Vector2(t, 0.0f);

        const Vector3 normal = Vector3(c, 0.0f, s).Normalize();
        v[bottomIdx].normal = normal;
        v[topIdx].normal = normal;
    }

    for (UINT seg = 0; seg < segmentCount_; ++seg) {
        const Index i0 = static_cast<Index>(seg * 2);
        const Index i1 = static_cast<Index>(seg * 2 + 1);
        const Index i2 = static_cast<Index>((seg + 1) * 2);
        const Index i3 = static_cast<Index>((seg + 1) * 2 + 1);

        i[iIndex++] = i0;
        i[iIndex++] = i1;
        i[iIndex++] = i3;

        i[iIndex++] = i0;
        i[iIndex++] = i3;
        i[iIndex++] = i2;
    }

    auto writeCap = [&](bool isTop) {
        const float y = isTop ? halfHeight : -halfHeight;
        const Vector3 normal = isTop ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, -1.0f, 0.0f);

        const size_t centerIndex = vIndex++;
        v[centerIndex].position = Vector4(0.0f, y, 0.0f, 1.0f);
        v[centerIndex].texcoord = Vector2(0.5f, 0.5f);
        v[centerIndex].normal = normal;

        const size_t ringStart = vIndex;
        for (UINT seg = 0; seg <= segmentCount_; ++seg) {
            const float t = denom > 0.0f ? static_cast<float>(seg) / denom : 0.0f;
            const float angle = t * twoPi;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const float x = c * radius;
            const float z = s * radius;

            v[vIndex].position = Vector4(x, y, z, 1.0f);
            v[vIndex].texcoord = Vector2((x / (radius * 2.0f)) + 0.5f, 0.5f - (z / (radius * 2.0f)));
            v[vIndex].normal = normal;
            ++vIndex;
        }

        for (UINT seg = 0; seg < segmentCount_; ++seg) {
            const Index ring0 = static_cast<Index>(ringStart + seg);
            const Index ring1 = static_cast<Index>(ringStart + seg + 1);
            const Index center = static_cast<Index>(centerIndex);

            if (isTop) {
                i[iIndex++] = center;
                i[iIndex++] = ring1;
                i[iIndex++] = ring0;
            } else {
                i[iIndex++] = center;
                i[iIndex++] = ring0;
                i[iIndex++] = ring1;
            }
        }
    };

    if (HasCap(caps_, Caps::Top)) {
        writeCap(true);
    }

    if (HasCap(caps_, Caps::Bottom)) {
        writeCap(false);
    }

    if (vIndex < vc) {
        for (size_t idx = vIndex; idx < vc; ++idx) {
            v[idx].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            v[idx].texcoord = Vector2(0.0f, 0.0f);
            v[idx].normal = Vector3(0.0f, 1.0f, 0.0f);
        }
    }

    if (iIndex < ic) {
        for (size_t idx = iIndex; idx < ic; ++idx) {
            i[idx] = 0;
        }
    }
}

bool Cylinder3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Cylinder3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
