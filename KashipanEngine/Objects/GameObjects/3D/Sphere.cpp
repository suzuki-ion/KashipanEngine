#include "Sphere.h"
#include <numbers>
#include <cmath>

namespace KashipanEngine {

Sphere::Sphere(size_t latSegments, size_t lonSegments, const std::string &name)
    : Object3DBase(name, sizeof(Vertex), sizeof(Index), (latSegments + 1) * (lonSegments + 1), latSegments * lonSegments * 6) {
    LogScope scope;

    const UINT vc = GetVertexCount();
    const UINT ic = GetIndexCount();
    if (vc == 0 || ic == 0) return;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < vc || i.size() < ic) return;

    const UINT latPlus1 = static_cast<UINT>(latSegments + 1);
    const UINT lonPlus1 = static_cast<UINT>(lonSegments + 1);
    const UINT latSeg = static_cast<UINT>(latSegments);
    const UINT lonSeg = static_cast<UINT>(lonSegments);

    if (latPlus1 < 2 || lonPlus1 < 2) return;

    const float pi = std::numbers::pi_v<float>;
    const float twoPi = 2.0f * pi;

    for (UINT lat = 0; lat < latPlus1; ++lat) {
        const float vT = latSeg > 0 ? static_cast<float>(lat) / static_cast<float>(latSeg) : 0.0f;
        const float theta = vT * pi; // 0..pi
        const float y = std::cos(theta) * 0.5f;
        const float r = std::sin(theta) * 0.5f;

        for (UINT lon = 0; lon < lonPlus1; ++lon) {
            const float uT = lonSeg > 0 ? static_cast<float>(lon) / static_cast<float>(lonSeg) : 0.0f;
            const float phi = uT * twoPi; // 0..2pi
            const float x = std::cos(phi) * r;
            const float z = std::sin(phi) * r;

            const UINT idx = lat * lonPlus1 + lon;
            v[idx].position = Vector4(x, y, z, 1.0f);
            v[idx].texcoord = Vector2(uT, vT);
        }
    }

    size_t w = 0;
    for (UINT lat = 0; lat < latSeg; ++lat) {
        for (UINT lon = 0; lon < lonSeg; ++lon) {
            const Index i0 = static_cast<Index>(lat * lonPlus1 + lon);
            const Index i1 = static_cast<Index>((lat + 1) * lonPlus1 + lon);
            const Index i2 = static_cast<Index>((lat + 1) * lonPlus1 + (lon + 1));
            const Index i3 = static_cast<Index>(lat * lonPlus1 + (lon + 1));

            i[w++] = i0;
            i[w++] = i2;
            i[w++] = i1;
            i[w++] = i0;
            i[w++] = i3;
            i[w++] = i2;
        }
    }
}

bool Sphere::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Sphere::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
