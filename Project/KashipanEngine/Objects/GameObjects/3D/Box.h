#pragma once
#include "Objects/EmptyObject.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Box : public EmptyObject {
public:
    Box();
    ~Box() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
