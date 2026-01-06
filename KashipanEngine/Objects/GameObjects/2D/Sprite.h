#pragma once
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Sprite : public Object2DBase {
public:
    Sprite();
    ~Sprite() override = default;

    /// @brief アンカーポイントの設定（(0,0)=左上、(0.5,0.5)=中央、(1,1)=右下） 
    void SetAnchorPoint(float x, float y);
    /// @brief アンカーポイントの設定（(0,0)=左上、(0.5,0.5)=中央、(1,1)=右下）
    void SetAnchorPoint(const Vector2 &anchor);
    const Vector2 &GetAnchorPoint() const { return anchorPoint_; }

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;

    void RebuildQuadVertices();

    Vector2 anchorPoint_{0.5f, 0.5f};
};

} // namespace KashipanEngine
