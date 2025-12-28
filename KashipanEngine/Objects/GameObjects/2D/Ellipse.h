#pragma once
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Ellipse : public Object2DBase {
public:
    /// @brief コンストラクタ
    /// @param segmentCount 楕円の分割数（頂点数に影響）
    Ellipse(size_t segmentCount = 32);
    ~Ellipse() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
