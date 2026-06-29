#pragma once
#include "Objects/EmptyObject.h"
#include "Objects/GameObjects/3D/VertexDataSkybox.h"

namespace KashipanEngine {

class Skybox : public EmptyObject {
public:
    Skybox();
    ~Skybox() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexDataSkybox;
    using Index = uint32_t;
};

} // namespace KashipanEngine
