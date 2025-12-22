#pragma once
#include "Objects/Object3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Box : public Object3DBase {
public:
    Box(const std::string &name = "Box") : Object3DBase(name, sizeof(Vertex), sizeof(Index), 24, 36) {}
    ~Box() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
