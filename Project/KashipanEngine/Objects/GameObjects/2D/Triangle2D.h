#pragma once
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Triangle2D : public Object2DBase {
public:
    Triangle2D();
    ~Triangle2D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
