#pragma once

#include "Objects/Object3DBase.h"
#include "Objects/GameObjects/3D/VertexData3D.h"

namespace KashipanEngine {

class Cylinder3D : public Object3DBase {
public:
    enum class Caps {
        None = 0,
        Bottom = 1,
        Top = 2,
        Both = 3,
    };

    /// @brief コンストラクタ
    /// @param segmentCount 側面の分割数
    /// @param caps 底面・上面の閉じ方
    /// @param height 高さ
    /// @param radius 半径
    Cylinder3D(size_t segmentCount = 16, Caps caps = Caps::Both, float height = 1.0f, float radius = 0.5f);
    ~Cylinder3D() override = default;

    void SetCaps(Caps caps);
    void SetHeight(float height);
    void SetRadius(float radius);

    Caps GetCaps() const { return caps_; }
    float GetHeight() const { return height_; }
    float GetRadius() const { return radius_; }

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    void UpdateGeometry();
    static bool HasCap(Caps caps, Caps flag);

    using Vertex = VertexData3D;
    using Index = uint32_t;

    size_t segmentCount_ = 0;
    float height_ = 1.0f;
    float radius_ = 0.5f;
    Caps caps_ = Caps::Both;
};

} // namespace KashipanEngine
