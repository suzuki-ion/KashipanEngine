#pragma once
#include "Objects/GameObjects/GameObject2DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Ellipse : public GameObject2DBase {
public:
    /// @brief コンストラクタ
    /// @param segmentCount 楕円の分割数（頂点数に影響）
    /// @param name オブジェクト名
    Ellipse(size_t segmentCount = 32, const std::string &name = "Ellipse")
        : GameObject2DBase(name, sizeof(Vertex), sizeof(Index), segmentCount, segmentCount * 3) {}
    ~Ellipse() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
