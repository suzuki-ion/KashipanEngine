#pragma once
#include <memory>
#include <optional>
#include "Core/Window.h"
#include "Graphics/Renderer.h"
#include "Graphics/Pipeline/System/PipelineBinder.h"
#include "Graphics/Pipeline/System/ShaderVariableBinder.h"
#include "Graphics/Resources.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

/// @brief 2Dゲームオブジェクト
class GameObject2D final {
public:
    /// @brief コンストラクタ（頂点数・インデックス数の指定が必須）
    GameObject2D(size_t vertexCount, size_t indexCount);

    GameObject2D(const GameObject2D&) = delete;
    GameObject2D& operator=(const GameObject2D&) = delete;
    GameObject2D(GameObject2D&&) = delete;
    GameObject2D& operator=(GameObject2D&&) = delete;
    virtual ~GameObject2D() = default;

    /// @brief レンダーパスの作成
    RenderPassInfo2D CreateRenderPass(Window *targetWindow,
        const std::string &pipelineName,
        const std::string &passName = "GameObject2D Render Pass");

private:
    /// @brief 描画処理
    virtual std::optional<RenderCommand> Draw(ShaderVariableBinder &shaderBinder, PipelineBinder &pipelineBinder);

    UINT vertexCount_ = 0;
    UINT indexCount_ = 0;
    std::unique_ptr<VertexBufferResource> vertexBuffer_;
    std::unique_ptr<IndexBufferResource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
    Vector4 *vertexData_ = nullptr;
};

} // namespace KashipanEngine
