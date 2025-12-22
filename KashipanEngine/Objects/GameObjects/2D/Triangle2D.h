#pragma once
#include "Objects/Object2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Triangle2D : public Object2DBase {
public:
    Triangle2D(const std::string &name = "Triangle2D");
    ~Triangle2D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
