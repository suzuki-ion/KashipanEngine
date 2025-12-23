#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"

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
    // テスト用の三角形オブジェクト
    static std::unique_ptr<Triangle3D> testObject3D;
    static Vector3 rotate(0.0f, 0.0f, 0.0f);
    // テスト用のカメラ
    static std::unique_ptr<Camera3D> testCamera3D;
    static bool initialized = false;
    
    //--------- 初期化 ---------//
    if (!initialized) {
        testObject3D = std::make_unique<Triangle3D>();
        testCamera3D = std::make_unique<Camera3D>();
        initialized = true;
    }
    
    //--------- テスト用の更新処理 ---------//
    {
        auto transformComp = testCamera3D->GetComponent3D<Transform3D>();
        if (transformComp) {
            transformComp->SetTranslate(Vector3(0.0f, 0.0f, -5.0f));
        }
    }
    rotate.y += 0.01f;
    {
        auto *transformComp = testObject3D->GetComponent3D<Transform3D>();
        if (transformComp) {
            transformComp->SetRotate(rotate);
        }
    }

    auto mainWindows = Window::GetWindows("Main Window");
    auto overlayWindows = Window::GetWindows("Overlay Window");
    if (!mainWindows.empty()) {
        auto *targetWindow = mainWindows.front();
        {
            auto passInfo = testCamera3D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Camera3D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testObject3D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Object2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
    }
    if (!overlayWindows.empty()) {
        auto* targetWindow = overlayWindows.front();
        {
            auto passInfo = testCamera3D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Camera3D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testObject3D->CreateRenderPass(targetWindow, "Graphics.Test", "Test Object2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
    }
    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
