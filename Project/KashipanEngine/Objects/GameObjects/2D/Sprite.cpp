#include "Sprite.h"

#include <algorithm>

#include "Objects/Components/2D/Transform2D.h"

namespace KashipanEngine {

Sprite::Sprite() : Object2DBase("Sprite", sizeof(Vertex), sizeof(Index), 4, 6) {
    SetRenderType(RenderType::Instancing);
    LogScope scope;

    RebuildQuadVertices();

    auto i = GetIndexSpan<Index>();
    if (i.size() >= 6) {
        i[0] = 0;
        i[1] = 1;
        i[2] = 2;
        i[3] = 0;
        i[4] = 2;
        i[5] = 3;
    }
}

void Sprite::SetPivotPoint(float x, float y) {
    Vector2 p{ x, y };
    p.x = std::clamp(p.x, 0.0f, 1.0f);
    p.y = std::clamp(p.y, 0.0f, 1.0f);

    if (pivotPoint_ == p) return;
    pivotPoint_ = p;
    RebuildQuadVertices();
}

void Sprite::SetPivotPoint(const Vector2 &pivot) {
    SetPivotPoint(pivot.x, pivot.y);
}

void Sprite::SetAnchorPoint(float x, float y) {
    Vector2 a{ x, y };
    a.x = std::clamp(a.x, 0.0f, 1.0f);
    a.y = std::clamp(a.y, 0.0f, 1.0f);

    if (anchorPoint_ == a) return;
    anchorPoint_ = a;
}

void Sprite::SetAnchorPoint(const Vector2 &anchor) {
    SetAnchorPoint(anchor.x, anchor.y);
}

void Sprite::RebuildQuadVertices() {
    const float ox = pivotPoint_.x - 0.5f;
    const float oy = 0.5f - pivotPoint_.y;

    auto v = GetVertexSpan<Vertex>();
    if (v.size() >= 4) {
        v[0].position = Vector4(-0.5f - ox, -0.5f - oy, 0.0f, 1.0f);
        v[1].position = Vector4(-0.5f - ox,  0.5f - oy, 0.0f, 1.0f);
        v[2].position = Vector4( 0.5f - ox,  0.5f - oy, 0.0f, 1.0f);
        v[3].position = Vector4( 0.5f - ox, -0.5f - oy, 0.0f, 1.0f);

        v[0].texcoord = Vector2(0.0f, 1.0f);
        v[1].texcoord = Vector2(0.0f, 0.0f);
        v[2].texcoord = Vector2(1.0f, 0.0f);
        v[3].texcoord = Vector2(1.0f, 1.0f);
    }
}

void Sprite::UpdateAnchorOffset() {
    auto *transform = GetComponent2D<Transform2D>();
    if (!transform) return;

    Vector3 anchorOffset{ 0.0f, 0.0f, 0.0f };
    if (auto *parent = transform->GetParentTransform()) {
        const Vector3 &parentScale = parent->GetScale();
        anchorOffset.x = (anchorPoint_.x - 0.5f) * parentScale.x;
        anchorOffset.y = (0.5f - anchorPoint_.y) * parentScale.y;
    }

    if (anchorOffset == appliedAnchorOffset_) return;

    Vector3 t = transform->GetTranslate();
    t -= appliedAnchorOffset_;
    t += anchorOffset;
    transform->SetTranslate(t);
    appliedAnchorOffset_ = anchorOffset;
}

bool Sprite::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    if (HasComponents2D("Material2D") == 0 ||
        HasComponents2D("Transform2D") == 0) {
        return false;
    }

    UpdateAnchorOffset();
    return true;
}

std::optional<RenderCommand> Sprite::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
