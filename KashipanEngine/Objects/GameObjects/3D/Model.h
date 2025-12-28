#pragma once
#include "Objects/Object3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Model : public Object3DBase {
public:
    Model(size_t vertexCount = 0, size_t indexCount = 0)
        : Object3DBase("Model", sizeof(Vertex), sizeof(Index), vertexCount, indexCount) {
        SetRenderType(RenderType::Instancing);
    }
    ~Model() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
