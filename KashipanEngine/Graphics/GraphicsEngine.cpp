#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"
#include "Math/Vector4.h"

namespace KashipanEngine {

GraphicsEngine::GraphicsEngine(Passkey<GameEngine>, DirectXCommon* directXCommon)
    : directXCommon_(directXCommon) {
    auto* device = directXCommon_->GetDevice(Passkey<GraphicsEngine>{});
    auto &settings = GetEngineSettings().rendering;
    pipelineManager_ = std::make_unique<PipelineManager>(Passkey<GraphicsEngine>{},
        device, settings.pipelineSettingsPath);
    Window::SetPipelineManager(Passkey<GraphicsEngine>{}, pipelineManager_.get());
    renderer_ = std::make_unique<Renderer>(Passkey<GraphicsEngine>{}, 1024, directXCommon_, pipelineManager_.get());
    Window::SetRenderer(Passkey<GraphicsEngine>{}, renderer_.get());
}

GraphicsEngine::~GraphicsEngine() = default;

void GraphicsEngine::RenderFrame(Passkey<GameEngine>) {
#pragma region TestDraw
    static bool isInitialized = false;
    static std::unique_ptr<VertexBufferResource> vertexBuffer;
    static D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    static RenderPassInfo2D testRenderPass;
    if (!isInitialized) {
        // テスト用頂点バッファ作成
        vertexBuffer = std::make_unique<VertexBufferResource>(sizeof(Vector4) * 3);
        Vector4 *vertexData = static_cast<Vector4 *>(vertexBuffer->Map());
        vertexData[0] = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
        vertexData[1] = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
        vertexData[2] = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
        vertexBuffer->Unmap();
        vertexBufferView = vertexBuffer->GetView(sizeof(Vector4));
        // テスト用レンダーパス作成
        testRenderPass.window = Window::GetWindows("Main Window").front();
        testRenderPass.pipelineName = "Graphics.Test";
        testRenderPass.passName = "Test Pass";
        testRenderPass.renderFunction = [&]([[maybe_unused]] ShaderVariableBinder &shaderBinder, [[maybe_unused]] PipelineBinder &pipelineBinder) -> RenderCommand {
            pipelineBinder.SetVertexBuffer(vertexBuffer.get(), sizeof(Vector4), 0);
            return RenderCommand{
                .vertexCount = 3,
                .indexCount = 0,
                .instanceCount = 1,
                .startVertexLocation = 0,
                .startIndexLocation = 0,
                .baseVertexLocation = 0,
                .startInstanceLocation = 0
            };
        };
        isInitialized = true;
    }
    renderer_->RegisterRenderPass(testRenderPass);
#pragma endregion
    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
