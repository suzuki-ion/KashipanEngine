#include "Billboard.h"

#include "FaceNormal3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Utilities/MathUtils.h"

#include <cmath>

namespace KashipanEngine {

Billboard::Billboard() : Object3DBase("Billboard", sizeof(Vertex), sizeof(Index), 4, 6) {
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

void Billboard::OnUpdate() {
    ApplyBillboardRotation();
}

void Billboard::ApplyBillboardRotation() {
    if (!camera_) return;

    auto *tr = GetComponent3D<Transform3D>();
    if (!tr) return;

    Vector3 rot = tr->GetRotate();

    if (facingMode_ == FacingMode::MatchCameraRotation) {
        if (!(isAutoRotateX_ || isAutoRotateY_ || isAutoRotateZ_)) return;

        auto *camTr = camera_->GetComponent3D<Transform3D>();
        if (!camTr) return;

        const auto &camRot = camTr->GetRotate();
        if (isAutoRotateX_) rot.x = camRot.x;
        if (isAutoRotateY_) rot.y = camRot.y;
        if (isAutoRotateZ_) rot.z = camRot.z;
        tr->SetRotate(rot);
        return;
    }

    const auto *camTr = camera_->GetComponent3D<Transform3D>();
    if (!camTr) return;

    const Vector3 selfPos = tr->GetTranslate();
    const Vector3 camPos = camTr->GetTranslate();
    Vector3 dir = camPos - selfPos;

    const float len2 = MathUtils::LengthSquared(dir);
    if (len2 == 0.0f) return;

    const float invLen = 1.0f / std::sqrt(len2);
    dir = dir * invLen;

    const float yaw = std::atan2(dir.x, dir.z) + 3.14159265358979323846f;

    const float pitch = std::asin(std::clamp(dir.y, -1.0f, 1.0f));

    if (isAutoRotateX_) rot.x = pitch;
    if (isAutoRotateY_) rot.y = yaw;
    if (isAutoRotateZ_) rot.z = 0.0f;

    tr->SetRotate(rot);
}

bool Billboard::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents3D("Transform3D") == 0 ||
        HasComponents3D("Material3D") == 0) {
        return false;
    }
    return true;
}

std::optional<RenderCommand> Billboard::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
