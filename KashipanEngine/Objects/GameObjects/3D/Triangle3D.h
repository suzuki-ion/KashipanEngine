#pragma once
#include "Objects/GameObjects/GameObject3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Triangle3D : public GameObject3DBase {
public:
    Triangle3D(const std::string &name = "Triangle3D") : GameObject3DBase(name, sizeof(Vertex), sizeof(Index), 3, 3) {}
    ~Triangle3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
