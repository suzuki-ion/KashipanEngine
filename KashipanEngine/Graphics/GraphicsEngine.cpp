#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"
#include "Math/Vector4.h"
#include "Objects/GameObjects/2D/Triangle2D.h"

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
    // テスト用の三角形オブジェクトを一つだけ生成
    static std::unique_ptr<Triangle2D> testObject2D;
    static bool initialized = false;
    if (!initialized) {
        testObject2D = std::make_unique<Triangle2D>();
        initialized = true;
    }
    auto mainWindows = Window::GetWindows("Main Window");
    auto overlayWindows = Window::GetWindows("Overlay Window");
    if (!mainWindows.empty()) {
        auto *targetWindow = mainWindows.front();
        auto passInfo = testObject2D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Object2D Pass");
        renderer_->RegisterRenderPass(passInfo);
    }
    if (!overlayWindows.empty()) {
        auto* targetWindow = overlayWindows.front();
        auto passInfo = testObject2D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Object2D Pass");
        renderer_->RegisterRenderPass(passInfo);
    }
    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
