#pragma once
#include "Objects/GameObjects/GameObject3DBase.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

class Line3D : public GameObject3DBase {
public:
    Line3D() = delete;
    /// @brief コンストラクタ
    /// @param lineCount ラインの数
    /// @param name オブジェクト名
    Line3D(size_t lineCount = 1, const std::string &name = "Line3D") : GameObject3DBase(name, sizeof(Vertex), sizeof(Index), lineCount + 1, lineCount + 1) {}
    ~Line3D() override = default;

protected:
    bool Render([[maybe_unused]] ShaderVariableBinder &shaderBinder) override;
    std::optional<RenderCommand> CreateRenderCommand(PipelineBinder &pipelineBinder) override;

private:
    using Vertex = Vector4;
    using Index = uint32_t;
};

} // namespace KashipanEngine
