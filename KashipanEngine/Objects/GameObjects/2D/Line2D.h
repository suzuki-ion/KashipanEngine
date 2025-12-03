#pragma once
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Line2D : public GameObject2DBase {
public:
    /// @brief コンストラクタ
    /// @param lineCount ラインの数
    /// @param name オブジェクト名
    Line2D(size_t lineCount = 1, const std::string &name = "Line2D") : GameObject2DBase(name, sizeof(Vertex), sizeof(Index), lineCount + 1, lineCount + 1) {}
    ~Line2D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
