#pragma once
#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Sphere : public Object3DBase {
public:
    /// @brief コンストラクタ
    /// @param latSegments 緯度分割数
    /// @param lonSegments 経度分割数
    Sphere(size_t latSegments = 8, size_t lonSegments = 16);
    ~Sphere() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
