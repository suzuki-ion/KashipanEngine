#include "GraphicsEngine.h"
#include "Core/DirectXCommon.h"
#include "Core/Window.h"
#include "Graphics/PipelineManager.h"
#include "Graphics/Renderer.h"
#include "EngineSettings.h"
#include "Graphics/Resources.h"

#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/2D/Ellipse.h"
#include "Objects/GameObjects/2D/Rect.h"
#include "Objects/GameObjects/2D/Sprite.h"
#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/Components/2D/Transform2D.h"

#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Box.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include <imgui.h>

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
    // テスト用の3Dオブジェクト
    static std::unique_ptr<Triangle3D> testTriangle3D1;
    static std::unique_ptr<Triangle3D> testTriangle3D2;
    static std::unique_ptr<Sphere> testSphere;
    static std::unique_ptr<Box> testBox;
    // テスト用の2Dオブジェクト
    static std::unique_ptr<Triangle2D> testTriangle2D;
    static std::unique_ptr<Ellipse> testEllipse;
    static std::unique_ptr<Rect> testRect;
    static std::unique_ptr<Sprite> testSprite;
    // テスト用のカメラ
    static std::unique_ptr<Camera3D> testCamera3D;
    static std::unique_ptr<Camera2D> testCamera2D;
    static bool initialized = false;
    
    //--------- 初期化 ---------//
    if (!initialized) {
        //==================================================
        // 3Dオブジェクトの初期化
        //==================================================

        testTriangle3D1 = std::make_unique<Triangle3D>();
        testTriangle3D2 = std::make_unique<Triangle3D>();
        // testObject3D2のY軸回転を少しずらす
        {
            auto *transformComp = testTriangle3D2->GetComponent3D<Transform3D>();
            if (transformComp) {
                transformComp->SetRotate(Vector3(0.0f, 0.5f, 0.0f));
            }
        }

        testSphere = std::make_unique<Sphere>();
        {
            auto *transformComp = testSphere->GetComponent3D<Transform3D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
            }
        }
        testBox = std::make_unique<Box>();
        {
            auto *transformComp = testBox->GetComponent3D<Transform3D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));
            }
        }

        testCamera3D = std::make_unique<Camera3D>();
        {
            auto transformComp = testCamera3D->GetComponent3D<Transform3D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector3(0.0f, 0.0f, -10.0f));
            }
        }

        //==================================================
        // 2Dオブジェクトの初期化
        //==================================================

        testTriangle2D = std::make_unique<Triangle2D>();
        {
            auto *transformComp = testTriangle2D->GetComponent2D<Transform2D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector2(50.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }
        testEllipse = std::make_unique<Ellipse>();
        {
            auto *transformComp = testEllipse->GetComponent2D<Transform2D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector2(150.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }
        testRect = std::make_unique<Rect>();
        {
            auto *transformComp = testRect->GetComponent2D<Transform2D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector2(250.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }
        testSprite = std::make_unique<Sprite>();
        {
            auto *transformComp = testSprite->GetComponent2D<Transform2D>();
            if (transformComp) {
                transformComp->SetTranslate(Vector2(350.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }
        testCamera2D = std::make_unique<Camera2D>();
        initialized = true;
    }
    
    //--------- テスト用の更新処理 ---------//
    {
        auto *transformComp = testTriangle3D1->GetComponent3D<Transform3D>();
        if (transformComp) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }
    {
        auto *transformComp = testTriangle3D2->GetComponent3D<Transform3D>();
        if (transformComp) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }

    // カメラをImGuiで操作可能にする
    {
        auto *transformComp = testCamera3D->GetComponent3D<Transform3D>();
        if (transformComp) {
            ImGui::Begin("Test Camera3D Control");
            Vector3 translate = transformComp->GetTranslate();
            Vector3 rotate = transformComp->GetRotate();
            ImGui::DragFloat3("Camera Translate", &translate.x, 0.05f);
            ImGui::DragFloat3("Camera Rotate", &rotate.x, 0.02f, -3.14f, 3.14f);
            transformComp->SetTranslate(translate);
            transformComp->SetRotate(rotate);
            ImGui::End();
        }
    }

    auto mainWindows = Window::GetWindows("Main Window");
    auto overlayWindows = Window::GetWindows("Overlay Window");
    if (!mainWindows.empty()) {
        auto *targetWindow = mainWindows.front();
        // 3Dオブジェクト描画パス登録
        {
            auto passInfo = testCamera3D->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Camera3D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle3D1->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Object2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle3D2->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Object2D Pass 2");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testSphere->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Sphere Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testBox->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Box Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        // 2Dオブジェクト描画パス登録
        {
            auto passInfo = testCamera2D->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Camera2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle2D->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Triangle2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testEllipse->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Ellipse Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testRect->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Rect Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testSprite->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Sprite Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
    }
    if (!overlayWindows.empty()) {
        auto* targetWindow = overlayWindows.front();
        // 3Dオブジェクト描画パス登録
        {
            auto passInfo = testCamera3D->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Camera3D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle3D1->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Object2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle3D2->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Object2D Pass 2");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testSphere->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Sphere Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testBox->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Box Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        // 2Dオブジェクト描画パス登録
        {
            auto passInfo = testCamera2D->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Camera2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testTriangle2D->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Triangle2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testEllipse->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Ellipse Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testRect->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Rect Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
        {
            auto passInfo = testSprite->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Sprite Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
    }
    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
