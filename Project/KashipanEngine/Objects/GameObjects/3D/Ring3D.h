#pragma once

#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Ring3D : public Object3DBase {
public:
    enum class UvMode {
        Default,
        Curved
    };

    /// @brief コンストラクタ
    /// @param segmentCount リングの分割数
    /// @param innerRadius 内径
    /// @param outerRadius 外径
    /// @param startAngle 開始角度（ラジアン）
    /// @param endAngle 終了角度（ラジアン）
    Ring3D(size_t segmentCount = 32, float innerRadius = 0.25f, float outerRadius = 0.5f,
        float startAngle = 0.0f, float endAngle = 2.0f * 3.14159265358979323846f,
        UvMode uvMode = UvMode::Default);
    ~Ring3D() override = default;

    void SetInnerRadius(float innerRadius);
    void SetOuterRadius(float outerRadius);
    void SetStartAngle(float startAngle);
    void SetEndAngle(float endAngle);
    void SetUvMode(UvMode uvMode);

    float GetInnerRadius() const { return innerRadius_; }
    float GetOuterRadius() const { return outerRadius_; }
    float GetStartAngle() const { return startAngle_; }
    float GetEndAngle() const { return endAngle_; }
    UvMode GetUvMode() const { return uvMode_; }

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    void UpdateVertices();
    void GenerateIndices();

    using Vertex = VertexData3D;
    using Index = uint32_t;

    size_t segmentCount_ = 0;
    float innerRadius_ = 0.25f;
    float outerRadius_ = 0.5f;
    float startAngle_ = 0.0f;
    float endAngle_ = 0.0f;
    UvMode uvMode_ = UvMode::Default;
};

} // namespace KashipanEngine
