#pragma once
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Triangle2D : public GameObject2DBase {
public:
    Triangle2D(const std::string &name = "Triangle2D");
    ~Triangle2D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;

    using ColorBuffer = Vector4;
    std::unique_ptr<ConstantBufferResource> colorBuffer_;
};

} // namespace KashipanEngine
