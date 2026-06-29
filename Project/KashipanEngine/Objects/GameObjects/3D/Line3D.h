#pragma once
#include "Objects/EmptyObject.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Line3D : public EmptyObject {
public:
    Line3D() = delete;
    /// @brief コンストラクタ
    /// @param lineCount ラインの数
    Line3D(size_t lineCount = 1);
    ~Line3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData3D;
    using Index = uint32_t;
};

} // namespace KashipanEngine
