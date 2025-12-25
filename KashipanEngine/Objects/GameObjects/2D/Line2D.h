#pragma once
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Line2D : public Object2DBase {
public:
    /// @brief コンストラクタ
    /// @param lineCount ラインの数
    /// @param name オブジェクト名
    Line2D(size_t lineCount = 1, const std::string &name = "Line2D");
    ~Line2D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
