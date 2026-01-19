#include "Sphere.h"
#include <numbers>
#include <cmath>
#include "FaceNormal3D.h"
#include "Objects/Components/3D/Material3D.h"

namespace KashipanEngine {

Sphere::Sphere(size_t latSegments, size_t lonSegments)
    : Object3DBase("Sphere", sizeof(Vertex), sizeof(Index), latSegments * lonSegments * 6, latSegments * lonSegments * 6) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    const UINT vc = GetVertexCount();
    const UINT ic = GetIndexCount();
    if (vc == 0 || ic == 0) return;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < vc || i.size() < ic) return;

    if (latSegments == 0 || lonSegments == 0) return;

    const float pi = std::numbers::pi_v<float>;
    const float twoPi = 2.0f * pi;

    auto makeVertex = [&](float theta, float phi, float uT, float vT) -> Vertex {
        const float y = std::cos(theta) * 0.5f;
        const float r = std::sin(theta) * 0.5f;
        const float x = std::cos(phi) * r;
        const float z = std::sin(phi) * r;
        Vertex out;
        out.position = Vector4(x, y, z, 1.0f);
        out.texcoord = Vector2(uT, vT);
        out.normal = Vector3(0.0f);
        return out;
    };

    size_t w = 0;
    for (UINT lat = 0; lat < static_cast<UINT>(latSegments); ++lat) {
        const float vT0 = static_cast<float>(lat) / static_cast<float>(latSegments);
        const float vT1 = static_cast<float>(lat + 1) / static_cast<float>(latSegments);
        const float theta0 = vT0 * pi;
        const float theta1 = vT1 * pi;

        for (UINT lon = 0; lon < static_cast<UINT>(lonSegments); ++lon) {
            const float uT0 = static_cast<float>(lon) / static_cast<float>(lonSegments);
            const float uT1 = static_cast<float>(lon + 1) / static_cast<float>(lonSegments);
            const float phi0 = uT0 * twoPi;
            const float phi1 = uT1 * twoPi;

            Vertex v00 = makeVertex(theta0, phi0, uT0, vT0);
            Vertex v10 = makeVertex(theta1, phi0, uT0, vT1);
            Vertex v11 = makeVertex(theta1, phi1, uT1, vT1);
            Vertex v01 = makeVertex(theta0, phi1, uT1, vT0);

            {
                const Vector3 n = ComputeFaceNormal(v00.position, v11.position, v10.position);
                v00.normal = n;
                v11.normal = n;
                v10.normal = n;

                v[w] = v00;
                i[w] = static_cast<Index>(w);
                ++w;

                v[w] = v11;
                i[w] = static_cast<Index>(w);
                ++w;

                v[w] = v10;
                i[w] = static_cast<Index>(w);
                ++w;
            }
            {
                const Vector3 n = ComputeFaceNormal(v00.position, v01.position, v11.position);
                v00.normal = n;
                v01.normal = n;
                v11.normal = n;

                v[w] = v00;
                i[w] = static_cast<Index>(w);
                ++w;

                v[w] = v01;
                i[w] = static_cast<Index>(w);
                ++w;

                v[w] = v11;
                i[w] = static_cast<Index>(w);
                ++w;
            }
        }
    }

    if (auto *material = GetComponent3D<Material3D>()) {
        material->SetEnableShadowMapProjection(false);
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
