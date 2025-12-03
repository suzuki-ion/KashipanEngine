#pragma once
#include "Objects/GameObjects/GameObject3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Box : public GameObject3DBase {
public:
    Box(const std::string &name = "Box") : GameObject3DBase(name, sizeof(Vertex), sizeof(Index), 24, 36) {}
    ~Box() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
