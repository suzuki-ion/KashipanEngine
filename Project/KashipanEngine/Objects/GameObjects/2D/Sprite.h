#pragma once
#include "Math/Vector3.h"
#include "Objects/Object2DBase.h"
#include "Objects/GameObjects/2D/VertexData2D.h"

namespace KashipanEngine {

class Sprite : public Object2DBase {
public:
    Sprite();
    ~Sprite() override = default;

    /// @brief ピボットポイントの設定（(0,0)=左上、(0.5,0.5)=中央、(1,1)=右下）
    void SetPivotPoint(float x, float y);
    /// @brief ピボットポイントの設定（(0,0)=左上、(0.5,0.5)=中央、(1,1)=右下）
    void SetPivotPoint(const Vector2 &pivot);
    const Vector2 &GetPivotPoint() const { return pivotPoint_; }

    /// @brief アンカーポイントの設定（親のどの位置を基準にするか）
    /// (0,0)=親の左上、(0.5,0.5)=親の中央、(1,1)=親の右下
    void SetAnchorPoint(float x, float y);
    /// @brief アンカーポイントの設定（親のどの位置を基準にするか）
    void SetAnchorPoint(const Vector2 &anchor);
    const Vector2 &GetAnchorPoint() const { return anchorPoint_; }

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = VertexData2D;
    using Index = uint32_t;

    void RebuildQuadVertices();
    void UpdateAnchorOffset();

    Vector2 pivotPoint_{0.5f, 0.5f};
    Vector2 anchorPoint_{0.5f, 0.5f};
    Vector3 appliedAnchorOffset_{0.0f, 0.0f, 0.0f};
};

} // namespace KashipanEngine
