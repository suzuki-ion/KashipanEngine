#pragma once
#include "Objects/Object2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Rect : public Object2DBase {
public:
    Rect(const std::string &name = "Rect") : Object2DBase(name, sizeof(Vertex), sizeof(Index), 4, 6) {}
    ~Rect() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
