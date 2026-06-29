#pragma once

#include "Objects/EmptyObject.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Plane3D : public EmptyObject {
public:
    Plane3D();
    ~Plane3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = std::uint32_t;
};

} // namespace KashipanEngine
