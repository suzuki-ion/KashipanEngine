#pragma once
#include "Objects/GameObjects/GameObject3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Model : public GameObject3DBase {
public:
    Model(size_t vertexCount = 0, size_t indexCount = 0, const std::string &name = "Model")
        : GameObject3DBase(name, sizeof(Vertex), sizeof(Index), vertexCount, indexCount) {}
    ~Model() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
