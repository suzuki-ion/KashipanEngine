#include "Plane3D.h"

#include "FaceNormal3D.h"

namespace KashipanEngine {

Plane3D::Plane3D() : Object3DBase("Plane3D", sizeof(Vertex), sizeof(Index), 4, 6) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < 4 || i.size() < 6) return;

    v[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
    v[1].position = Vector4(-0.5f,  0.5f, 0.0f, 1.0f);
    v[2].position = Vector4( 0.5f,  0.5f, 0.0f, 1.0f);
    v[3].position = Vector4( 0.5f, -0.5f, 0.0f, 1.0f);

    v[0].texcoord = Vector2(0.0f, 1.0f);
    v[1].texcoord = Vector2(0.0f, 0.0f);
    v[2].texcoord = Vector2(1.0f, 0.0f);
    v[3].texcoord = Vector2(1.0f, 1.0f);

    const Vector3 n = ComputeFaceNormal(v[0].position, v[1].position, v[2].position);
    v[0].normal = n;
    v[1].normal = n;
    v[2].normal = n;
    v[3].normal = n;

    i[0] = 0;
    i[1] = 1;
    i[2] = 2;
    i[3] = 0;
    i[4] = 2;
    i[5] = 3;
}

bool Plane3D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Plane3D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
