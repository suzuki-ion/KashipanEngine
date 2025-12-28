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

#if defined(USE_IMGUI)
#include <imgui.h>
#include "Utilities/Translation.h"
#endif

#include <unordered_map>

namespace KashipanEngine {

namespace {
constexpr bool kEnableInstancingTest = true;

// Number of objects to generate for instancing verification
constexpr std::uint32_t kInstancingTestCount2D = 128;
constexpr std::uint32_t kInstancingTestCount3D = 128;

// Shared batch keys (must be non-zero to enable Renderer batching)
constexpr std::uint64_t kBatchKey2D = 0x2D2D2D2D2D2D2D2Dull;
constexpr std::uint64_t kBatchKey3D = 0x3D3D3D3D3D3D3D3Dull;

} // namespace

GraphicsEngine::GraphicsEngine(Passkey<GameEngine>, DirectXCommon *directXCommon)
    : directXCommon_(directXCommon) {
    auto *device = directXCommon_->GetDevice(Passkey<GraphicsEngine>{});
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
        testObjects3D.reserve(2 + (kEnableInstancingTest ? kInstancingTestCount3D : 0));

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

        // インスタンシング用の三角形
        {
            const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount3D : 0;
            Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 3DCount=" + std::to_string(instanceCount),
                LogSeverity::Info);
            for (std::uint32_t i = 0; i < kInstancingTestCount3D; ++i) {
                auto obj = std::make_unique<Triangle3D>();
                obj->SetName(std::string("InstancingTriangle3D_") + std::to_string(i));

                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    const float x = (static_cast<float>(i % 16) - 8.0f) * 0.4f;
                    const float y = (static_cast<float>(i / 16) - 2.0f) * 0.4f;
                    tr->SetTranslate(Vector3(x, y, 0.0f));
                }

                testObjects3D.emplace_back(std::move(obj));
            }
        }

        //// Triangle3D 1
        //testObjects3D.emplace_back(std::make_unique<Triangle3D>());

        //// Triangle3D 2
        //testObjects3D.emplace_back(std::make_unique<Triangle3D>());
        //if (auto *tri = static_cast<Triangle3D *>(testObjects3D.back().get())) {
        //    if (auto *transformComp = tri->GetComponent3D<Transform3D>()) {
        //        transformComp->SetRotate(Vector3(0.0f, 0.5f, 0.0f));
        //    }
        //}

        //// Sphere
        //testObjects3D.emplace_back(std::make_unique<Sphere>());
        //if (auto *sphere = static_cast<Sphere *>(testObjects3D.back().get())) {
        //    if (auto *transformComp = sphere->GetComponent3D<Transform3D>()) {
        //        transformComp->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
        //    }
        //}

        //// Box
        //testObjects3D.emplace_back(std::make_unique<Box>());
        //if (auto *box = static_cast<Box *>(testObjects3D.back().get())) {
        //    if (auto *transformComp = box->GetComponent3D<Transform3D>()) {
        //        transformComp->SetTranslate(Vector3(-2.0f, 0.0f, 0.0f));
        //    }
        //}

        //==================================================
        // 2Dオブジェクトの初期化
        //==================================================
        testObjects2D.clear();
        testObjects2D.reserve(1 + (kEnableInstancingTest ? kInstancingTestCount2D : 0));

        // Camera2D
        testObjects2D.emplace_back(std::make_unique<Camera2D>());

        // インスタンシング用の三角形
        {
            const uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount2D : 0;
            Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 2DCount=" + std::to_string(instanceCount),
                LogSeverity::Info);
            for (std::uint32_t i = 0; i < kInstancingTestCount2D; ++i) {
                auto obj = std::make_unique<Triangle2D>();
                obj->SetName(std::string("InstancingTriangle2D_") + std::to_string(i));
                if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                    const float x = 50.0f + static_cast<float>(i % 16) * 30.0f;
                    const float y = 200.0f + static_cast<float>(i / 16) * 30.0f;
                    tr->SetTranslate(Vector2(x, y));
                    tr->SetScale(Vector2(20.0f, 20.0f));
                }
                testObjects2D.emplace_back(std::move(obj));
            }
        }

        //// Triangle2D
        //testObjects2D.emplace_back(std::make_unique<Triangle2D>());
        //if (auto *tri = static_cast<Triangle2D *>(testObjects2D.back().get())) {
        //    if (auto *transformComp = tri->GetComponent2D<Transform2D>()) {
        //        transformComp->SetTranslate(Vector2(50.0f, 50.0f));
        //        transformComp->SetScale(Vector2(100.0f, 100.0f));
        //    }
        //}

        //// Ellipse
        //testObjects2D.emplace_back(std::make_unique<Ellipse>());
        //if (auto *ellipse = static_cast<Ellipse *>(testObjects2D.back().get())) {
        //    if (auto *transformComp = ellipse->GetComponent2D<Transform2D>()) {
        //        transformComp->SetTranslate(Vector2(150.0f, 50.0f));
        //        transformComp->SetScale(Vector2(100.0f, 100.0f));
        //    }
        //}

        //// Rect
        //testObjects2D.emplace_back(std::make_unique<Rect>());
        //if (auto *rect = static_cast<Rect *>(testObjects2D.back().get())) {
        //    if (auto *transformComp = rect->GetComponent2D<Transform2D>()) {
        //        transformComp->SetTranslate(Vector2(250.0f, 50.0f));
        //        transformComp->SetScale(Vector2(100.0f, 100.0f));
        //    }
        //}

        //// Sprite
        //testObjects2D.emplace_back(std::make_unique<Sprite>());
        //if (auto *sprite = static_cast<Sprite *>(testObjects2D.back().get())) {
        //    if (auto *transformComp = sprite->GetComponent2D<Transform2D>()) {
        //        transformComp->SetTranslate(Vector2(350.0f, 50.0f));
        //        transformComp->SetScale(Vector2(100.0f, 100.0f));
        //    }
        //}

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

#if defined(USE_IMGUI)
    // テスト用：オブジェクト/コンポーネントのパラメータ調整ウィンドウ
    if (ImGui::Begin(Translation("engine.imgui.testObjectInspector.title").c_str())) {
        if (ImGui::CollapsingHeader(Translation("engine.imgui.testObjectInspector.objects3d").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int i = 0;
            for (auto &obj : testObjects3D) {
                if (!obj) continue;
                if (ImGui::TreeNode((std::to_string(i++) + ' ' + obj->GetName()).c_str())) {
                    obj->ShowImGui();
                    ImGui::TreePop();
                }
            }
        }

        if (ImGui::CollapsingHeader(Translation("engine.imgui.testObjectInspector.objects2d").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            int i = 0;
            for (auto &obj : testObjects2D) {
                if (!obj) continue;
                if (ImGui::TreeNode((std::to_string(i++) + ' ' + obj->GetName()).c_str())) {
                    obj->ShowImGui();
                    ImGui::TreePop();
                }
            }
        }
    }
    ImGui::End();
#endif

    auto mainWindow = Window::GetWindow("Main Window");
    auto overlayWindow = Window::GetWindow("Overlay Window");

    // Find system objects
    Camera3D *camera3D = nullptr;
    DirectionalLight *dirLight = nullptr;
    for (auto &obj : testObjects3D) {
        if (!obj) continue;
        if (!camera3D && obj->GetName() == "Camera3D") camera3D = static_cast<Camera3D *>(obj.get());
        if (!dirLight && obj->GetName() == "DirectionalLight") dirLight = static_cast<DirectionalLight *>(obj.get());
    }

    Camera2D *camera2D = nullptr;
    for (auto &obj : testObjects2D) {
        if (!obj) continue;
        if (!camera2D && obj->GetName() == "Camera2D") camera2D = static_cast<Camera2D *>(obj.get());
    }

    std::vector<Window *> activeWindows;
    if (mainWindow) activeWindows.push_back(mainWindow);
    if (overlayWindow) activeWindows.push_back(overlayWindow);

    auto registerPasses = [&](Window *targetWindow) {
        if (!targetWindow) return;

        // 3Dオブジェクト描画パス登録
        {
            // Camera / Light constant buffers are configured via SystemObject-derived classes (not via RenderPass)
            if (camera3D && dirLight) {
                camera3D->ConfigureConstantBuffers({
                    {"Vertex:gCamera", sizeof(Camera3D::CameraBuffer)},
                    {"Pixel:gDirectionalLight", sizeof(DirectionalLight::LightBuffer)},
                },
                [camera3D, dirLight](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
                    if (!constantBufferMaps) return false;
                    if (!camera3D || !dirLight) return false;

                    camera3D->UpdateCameraBufferCPUForRenderer();
                    dirLight->UpdateLightBufferCPUForRenderer();

                    auto **maps = static_cast<void **>(constantBufferMaps);
                    const auto &cam = camera3D->GetCameraBufferCPU();
                    const auto &light = dirLight->GetLightBufferCPU();
                    std::memcpy(maps[0], &cam, sizeof(Camera3D::CameraBuffer));
                    std::memcpy(maps[1], &light, sizeof(DirectionalLight::LightBuffer));
                    return true;
                });
            }

            for (auto &obj : testObjects3D) {
                if (!obj) continue;
                auto passInfo = obj->CreateRenderPass(targetWindow, "Object3D.Solid.BlendNormal", obj->GetName() + " Pass");
                renderer_->RegisterRenderPass(passInfo);
            }
        }

        // 2Dオブジェクト描画パス登録
        {
            if (camera2D) {
                camera2D->ConfigureConstantBuffers({
                    {"Vertex:gCamera", sizeof(Camera2D::CameraBuffer)},
                },
                [camera2D](void *constantBufferMaps, std::uint32_t /*instanceCount*/) -> bool {
                    if (!constantBufferMaps) return false;
                    if (!camera2D) return false;

                    camera2D->UpdateCameraBufferCPUForRenderer();

                    auto **maps = static_cast<void **>(constantBufferMaps);
                    const auto &cam = camera2D->GetCameraBufferCPU();
                    std::memcpy(maps[0], &cam, sizeof(Camera2D::CameraBuffer));
                    return true;
                });
            }

            for (auto &obj : testObjects2D) {
                if (!obj) continue;
                auto passInfo = obj->CreateRenderPass(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal", obj->GetName() + " Pass");
                renderer_->RegisterRenderPass(passInfo);
            }
        }
    };

    /*if (mainWindow) {
        registerPasses(mainWindow);
    }*/
    if (overlayWindow) {
        registerPasses(overlayWindow);
    }

    renderer_->RenderFrame({});
}

} // namespace KashipanEngine
