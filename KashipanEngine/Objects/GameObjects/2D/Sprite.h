#pragma once
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Sprite : public Object2DBase {
public:
    Sprite(const std::string &name = "Sprite");
    ~Sprite() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
