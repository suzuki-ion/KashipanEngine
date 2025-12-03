#include "Triangle2D.h"

namespace KashipanEngine {

Triangle2D::Triangle2D() : GameObject2DBase(sizeof(Vertex), sizeof(Index), 3, 3) {
    LogScope scope;
    colorBuffer_ = std::make_unique<ConstantBufferResource>(sizeof(ColorBuffer));
    ColorBuffer *mappedColor = static_cast<ColorBuffer *>(colorBuffer_->Map());
    *mappedColor = ColorBuffer(1.0f, 1.0f, 1.0f, 1.0f);
}

bool Triangle2D::Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) {
    auto v = GetVertexSpan<Vertex>();
    if (v.size() < 3) return false;
    v[0] = Vertex(-0.5f, -0.5f, 0.0f, 1.0f);
    v[1] = Vertex(0.0f, 0.5f, 0.0f, 1.0f);
    v[2] = Vertex(0.5f, -0.5f, 0.0f, 1.0f);
    auto i = GetIndexSpan<Index>();
    if (i.size() < 3) return false;
    i[0] = 0;
    i[1] = 1;
    i[2] = 2;
    ColorBuffer *mappedColor = static_cast<ColorBuffer *>(colorBuffer_->Map());
    *mappedColor = ColorBuffer(1.0f, 1.0f, 0.0f, 1.0f);
    shaderBinder.Bind("Pixel:gMaterial", colorBuffer_.get());
    return true;
}

std::optional<RenderCommand> Triangle2D::CreateRenderCommand(PipelineBinder &pipelineBinder) {
    if (GetVertexCount() == 0 && GetIndexCount() == 0) return std::nullopt;
    SetVertexBuffer(pipelineBinder);
    SetIndexBuffer(pipelineBinder);
    return CreateDefaultRenderCommand();
}

} // namespace KashipanEngine
