#pragma once
#include "Objects/Object3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Triangle3D : public Object3DBase {
public:
    Triangle3D(const std::string &name = "Triangle3D");
    ~Triangle3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
