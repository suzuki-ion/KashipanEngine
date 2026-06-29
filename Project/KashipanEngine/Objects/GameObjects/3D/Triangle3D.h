#pragma once
#include "Objects/EmptyObject.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Triangle3D : public EmptyObject {
public:
    Triangle3D();
    ~Triangle3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
