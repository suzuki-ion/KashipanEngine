#pragma once
#include "Objects/GameObjects/GameObject3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Sphere : public GameObject3DBase {
public:
    /// @brief コンストラクタ
    /// @param latSegments 緯度分割数
    /// @param lonSegments 経度分割数
    /// @param name オブジェクト名
    Sphere(size_t latSegments = 8, size_t lonSegments = 16, const std::string &name = "Sphere")
        : GameObject3DBase(name, sizeof(Vertex), sizeof(Index), (latSegments + 1) * (lonSegments + 1), latSegments * lonSegments * 6) {}
    ~Sphere() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
