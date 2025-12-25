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
#include "Objects/SystemObjects/DirectionalLight.h"
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
    // テスト用（Object2DBase / Object3DBase の vector に格納できるか）
    static std::vector<std::unique_ptr<Object3DBase>> testObjects3D;
    static std::vector<std::unique_ptr<Object2DBase>> testObjects2D;
    static bool initialized = false;

    //--------- 初期化 ---------//
    if (!initialized) {
        //==================================================
        // 3Dオブジェクトの初期化
        //==================================================
        testObjects3D.clear();
        testObjects3D.reserve(6);

        // Camera3D
        testObjects3D.emplace_back(std::make_unique<Camera3D>());
        if (auto *camera = static_cast<Camera3D *>(testObjects3D.back().get())) {
            if (auto *transformComp = camera->GetComponent3D<Transform3D>()) {
                transformComp->SetTranslate(Vector3(0.0f, 0.0f, -10.0f));
            }
        }

        // DirectionalLight
        testObjects3D.emplace_back(std::make_unique<DirectionalLight>());
        if (auto *light = static_cast<DirectionalLight *>(testObjects3D.back().get())) {
            light->SetEnabled(true);
            light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            light->SetDirection(Vector3(0.3f, -1.0f, 0.2f));
            light->SetIntensity(1.0f);
        }

        // Triangle3D 1
        testObjects3D.emplace_back(std::make_unique<Triangle3D>());

        // Triangle3D 2
        testObjects3D.emplace_back(std::make_unique<Triangle3D>());
        if (auto *tri = static_cast<Triangle3D *>(testObjects3D.back().get())) {
            if (auto *transformComp = tri->GetComponent3D<Transform3D>()) {
                transformComp->SetRotate(Vector3(0.0f, 0.5f, 0.0f));
            }
        }

        // Sphere
        testObjects3D.emplace_back(std::make_unique<Sphere>());
        if (auto *sphere = static_cast<Sphere *>(testObjects3D.back().get())) {
            if (auto *transformComp = sphere->GetComponent3D<Transform3D>()) {
                transformComp->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
            }
        }

        // Box
        testObjects3D.emplace_back(std::make_unique<Box>());
        if (auto *box = static_cast<Box *>(testObjects3D.back().get())) {
            if (auto *transformComp = box->GetComponent3D<Transform3D>()) {
                transformComp->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));
            }
        }

        //==================================================
        // 2Dオブジェクトの初期化
        //==================================================
        testObjects2D.clear();
        testObjects2D.reserve(5);

        // Camera2D
        testObjects2D.emplace_back(std::make_unique<Camera2D>());

        // Triangle2D
        testObjects2D.emplace_back(std::make_unique<Triangle2D>());
        if (auto *tri = static_cast<Triangle2D *>(testObjects2D.back().get())) {
            if (auto *transformComp = tri->GetComponent2D<Transform2D>()) {
                transformComp->SetTranslate(Vector2(50.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }

        // Ellipse
        testObjects2D.emplace_back(std::make_unique<Ellipse>());
        if (auto *ellipse = static_cast<Ellipse *>(testObjects2D.back().get())) {
            if (auto *transformComp = ellipse->GetComponent2D<Transform2D>()) {
                transformComp->SetTranslate(Vector2(150.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }

        // Rect
        testObjects2D.emplace_back(std::make_unique<Rect>());
        if (auto *rect = static_cast<Rect *>(testObjects2D.back().get())) {
            if (auto *transformComp = rect->GetComponent2D<Transform2D>()) {
                transformComp->SetTranslate(Vector2(250.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }

        // Sprite
        testObjects2D.emplace_back(std::make_unique<Sprite>());
        if (auto *sprite = static_cast<Sprite *>(testObjects2D.back().get())) {
            if (auto *transformComp = sprite->GetComponent2D<Transform2D>()) {
                transformComp->SetTranslate(Vector2(350.0f, 50.0f));
                transformComp->SetScale(Vector2(100.0f, 100.0f));
            }
        }

        initialized = true;
    }

    //--------- テスト用の更新処理 ---------//

    // 3D: Camera / Light 以外は回転
    for (auto &obj : testObjects3D) {
        if (!obj) continue;
        if (obj->GetName() == "Camera3D") continue;
        if (obj->GetName() == "DirectionalLight") continue;

        if (auto *transformComp = obj->GetComponent3D<Transform3D>()) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }

    // カメラをImGuiで操作可能にする
    for (auto &obj : testObjects3D) {
        if (!obj) continue;
        if (obj->GetName() != "Camera3D") continue;
        auto *transformComp = obj->GetComponent3D<Transform3D>();
        ImGui::Begin("Test Camera3D Control");
        Vector3 translate = transformComp->GetTranslate();
        Vector3 rotate = transformComp->GetRotate();
        ImGui::DragFloat3("Camera Translate", &translate.x, 0.05f);
        ImGui::DragFloat3("Camera Rotate", &rotate.x, 0.02f, -3.14f, 3.14f);
        transformComp->SetTranslate(translate);
        transformComp->SetRotate(rotate);
        ImGui::End();
        break;
    }
    // 平行光源もImGuiで操作可能にする
    {
        for (auto &obj : testObjects3D) {
            if (!obj) continue;
            if (obj->GetName() != "DirectionalLight") continue;
            auto *light = static_cast<DirectionalLight *>(obj.get());
            ImGui::Begin("Test DirectionalLight Control");
            Vector3 direction = light->GetDirection();
            Vector4 color = light->GetColor();
            float intensity = light->GetIntensity();
            ImGui::DragFloat3("Light Direction", &direction.x, 0.05f);
            ImGui::ColorEdit4("Light Color", &color.x);
            ImGui::DragFloat("Light Intensity", &intensity, 0.1f, 0.0f, 10.0f);
            light->SetDirection(direction);
            light->SetColor(color);
            light->SetIntensity(intensity);
            ImGui::End();
            break;
        }
    }

    auto mainWindow = Window::GetWindow("Main Window");
    auto overlayWindow = Window::GetWindow("Overlay Window");

    auto registerPasses = [&](Window *targetWindow) {
        if (!targetWindow) return;

        // 3Dオブジェクト描画パス登録
        for (auto &obj : testObjects3D) {
            if (!obj) continue;
            auto passInfo = obj->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", "Test Object3D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }

        // 2Dオブジェクト描画パス登録
        for (auto &obj : testObjects2D) {
            if (!obj) continue;
            auto passInfo = obj->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", "Test Object2D Pass");
            renderer_->RegisterRenderPass(passInfo);
        }
    };

    if (mainWindow) {
        registerPasses(mainWindow);
    }
    if (overlayWindow) {
        registerPasses(overlayWindow);
    }

    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
