#include "Skybox.h"

namespace KashipanEngine {

Skybox::Skybox() : Object3DBase("Skybox", sizeof(Vertex), sizeof(Index), 24, 36) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    auto v = GetVertexSpan<Vertex>();
    auto i = GetIndexSpan<Index>();
    if (v.size() < 24 || i.size() < 36) return;

    const float s = 1.0f;
    // 8 vertices for the cube
    const Vector4 p[8] = {
        Vector4(-s, -s, -s, 1.0f),
        Vector4(-s,  s, -s, 1.0f),
        Vector4( s,  s, -s, 1.0f),
        Vector4( s, -s, -s, 1.0f),
        Vector4(-s, -s,  s, 1.0f),
        Vector4(-s,  s,  s, 1.0f),
        Vector4( s,  s,  s, 1.0f),
        Vector4( s, -s,  s, 1.0f),
    };

    // For skybox, texcoord is a direction vector (position)
    auto setFace = [&](size_t base, int a, int b, int c, int d) {
        v[base + 0] = Vertex{ p[a], Vector3(p[a].x, p[a].y, p[a].z) };
        v[base + 1] = Vertex{ p[b], Vector3(p[b].x, p[b].y, p[b].z) };
        v[base + 2] = Vertex{ p[c], Vector3(p[c].x, p[c].y, p[c].z) };
        v[base + 3] = Vertex{ p[d], Vector3(p[d].x, p[d].y, p[d].z) };
    };

    // -Z, +Z, -X, +X, +Y, -Y
    setFace(0, 0, 1, 2, 3);
    setFace(4, 4, 7, 6, 5);
    setFace(8, 4, 5, 1, 0);
    setFace(12, 3, 2, 6, 7);
    setFace(16, 1, 5, 6, 2);
    setFace(20, 4, 0, 3, 7);

    for (size_t f = 0; f < 6; ++f) {
        const Index baseV = static_cast<Index>(f * 4);
        const size_t baseI = f * 6;
        i[baseI + 0] = baseV + 0;
        i[baseI + 1] = baseV + 2;
        i[baseI + 2] = baseV + 1;
        i[baseI + 3] = baseV + 0;
        i[baseI + 4] = baseV + 3;
        i[baseI + 5] = baseV + 2;
    }
}

bool Skybox::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Skybox::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
