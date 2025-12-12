#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/Components/2D/Material2D.h"
#include "Objects/GameObjects/Components/2D/Transform2D.h"

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
    static Vector3 position(0.0f, 0.0f, 0.0f);
    static bool initialized = false;
    if (!initialized) {
        testObject2D = std::make_unique<Triangle2D>();
        testObject2D->RegisterComponent<Material2D<Vector4>>(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
        Matrix4x4 transform;
        transform.MakeTranslate(position);
        testObject2D->RegisterComponent<Transform2D<Matrix4x4>>(transform);
        initialized = true;
    }
    // sin派でX軸方向に移動
    position.x = std::sin(static_cast<float>(GetTickCount64()) / 1000.0f) * 0.5f;
    Matrix4x4 transform;
    transform.MakeTranslate(position);
    auto *transformComp = testObject2D->GetComponents2D("Transform2D").front();
    if (transformComp) {
        auto *t2d = static_cast<Transform2D<Matrix4x4>*>(transformComp);
        if (t2d) {
            t2d->SetTransform(transform);
        }
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
