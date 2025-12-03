#pragma once
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Sprite : public GameObject2DBase {
public:
    Sprite(const std::string &name = "Sprite") : GameObject2DBase(name, sizeof(Vertex), sizeof(Index), 4, 6) {}
    ~Sprite() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
