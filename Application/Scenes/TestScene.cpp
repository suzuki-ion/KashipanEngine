#include "Scenes/TestScene.h"

#include "Core/Window.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/GameObjects/3D/Triangle3D.h"
#include "Objects/GameObjects/3D/Model.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/2D/Triangle2D.h"
#include "Objects/GameObjects/2D/Sprite.h"

#include "Assets/ModelManager.h"
#include "Debug/Logger.h"

#include <algorithm>
#include <cstdint>

namespace KashipanEngine {
namespace {
constexpr bool kEnableInstancingTest = true;
constexpr std::uint32_t kInstancingTestCount2D = 64;
constexpr std::uint32_t kInstancingTestCount3D = 64;
} // namespace

TestScene::TestScene()
    : Scene("TestScene") {
}

TestScene::~TestScene() {
    objects2D_.clear();
    objects3D_.clear();
}

void TestScene::InitializeTestObjects() {
    if (initialized_) return;

    auto overlayWindow = Window::GetWindow("Overlay Window");
    auto *targetWindow = overlayWindow;

    if (!offscreenBuffer_ && targetWindow) {
        const std::uint32_t w = 1920;
        const std::uint32_t h = 1080;
        offscreenBuffer_ = ScreenBuffer::Create(targetWindow, w, h, RenderDimension::D2);
    }

    auto attachOffscreenIfPossible3D = [&](Object3DBase *obj) {
        if (!obj || !offscreenBuffer_) return;
        obj->AttachToRenderer(offscreenBuffer_, "Object3D.Solid.BlendNormal");
    };
    auto attachOffscreenIfPossible2D = [&](Object2DBase *obj) {
        if (!obj || !offscreenBuffer_) return;
        obj->AttachToRenderer(offscreenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
    };

    // 3D
    objects3D_.clear();
    objects3D_.reserve(2 + (kEnableInstancingTest ? kInstancingTestCount3D : 0) + 1);

    objects3D_.emplace_back(std::make_unique<Camera3D>());
    if (auto *camera = static_cast<Camera3D *>(objects3D_.back().get())) {
        if (auto *transformComp = camera->GetComponent3D<Transform3D>()) {
            transformComp->SetTranslate(Vector3(0.0f, 0.0f, -10.0f));
        }
    }
    attachOffscreenIfPossible3D(objects3D_.back().get());

    objects3D_.emplace_back(std::make_unique<DirectionalLight>());
    if (auto *light = static_cast<DirectionalLight *>(objects3D_.back().get())) {
        light->SetEnabled(true);
        light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        light->SetDirection(Vector3(0.3f, -1.0f, 0.2f));
        light->SetIntensity(1.0f);
    }
    attachOffscreenIfPossible3D(objects3D_.back().get());

    {
        const std::uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount3D : 0;
        Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 3DCount=" + std::to_string(instanceCount),
            LogSeverity::Info);

        for (std::uint32_t i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Triangle3D>();
            obj->SetName(std::string("InstancingTriangle3D_") + std::to_string(i));

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                const float x = (static_cast<float>(i % 16) - 8.0f) * 0.4f;
                const float y = (static_cast<float>(i / 16) - 2.0f) * 0.4f;
                tr->SetTranslate(Vector3(x, y, 0.0f));
            }
            attachOffscreenIfPossible3D(obj.get());
            objects3D_.emplace_back(std::move(obj));
        }
    }

    {
        ModelManager::ModelHandle modelHandle = ModelManager::GetModelHandleFromFileName("icoSphere.obj");
        if (modelHandle != ModelManager::kInvalidHandle) {
            auto obj = std::make_unique<Model>(modelHandle);
            obj->SetName("TestModel_IcoSphere");
            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3(2.0f, 0.0f, 0.0f));
                tr->SetScale(Vector3(0.5f, 0.5f, 0.5f));
            }
            attachOffscreenIfPossible3D(obj.get());
            objects3D_.emplace_back(std::move(obj));
        } else {
            Log("[TestScene] Test model 'icoSphere.obj' not found.", LogSeverity::Warning);
        }
    }

    // 2D
    objects2D_.clear();
    objects2D_.reserve(1 + (kEnableInstancingTest ? kInstancingTestCount2D : 0));

    objects2D_.emplace_back(std::make_unique<Camera2D>());
    objects2D_.back()->AttachToRenderer(overlayWindow, "Object2D.DoubleSidedCulling.BlendNormal");
    attachOffscreenIfPossible2D(objects2D_.back().get());

    {
        const std::uint32_t instanceCount = kEnableInstancingTest ? kInstancingTestCount2D : 0;
        Log(std::string("[InstancingTest] Setup started. instancing=")
                + (kEnableInstancingTest ? "true" : "false")
                + " 2DCount=" + std::to_string(instanceCount),
            LogSeverity::Info);

        for (std::uint32_t i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Triangle2D>();
            obj->SetName(std::string("InstancingTriangle2D_") + std::to_string(i));
            if (auto *tr = obj->GetComponent2D<Transform2D>()) {
                const float x = 50.0f + static_cast<float>(i % 16) * 30.0f;
                const float y = 200.0f + static_cast<float>(i / 16) * 30.0f;
                tr->SetTranslate(Vector2(x, y));
                tr->SetScale(Vector2(20.0f, 20.0f));
            }
            attachOffscreenIfPossible2D(obj.get());
            objects2D_.emplace_back(std::move(obj));
        }
    }

    // ScreenBufferをウィンドウに表示するためのスプライトを作成
    if (offscreenBuffer_ && targetWindow) {
        auto spriteObj = std::make_unique<Sprite>();
        spriteObj->SetName("OffscreenBufferDisplaySprite");
        if (auto *material = spriteObj->GetComponent2D<Material2D>()) {
            material->SetTexture(offscreenBuffer_);
        }
        spriteObj->AttachToRenderer(targetWindow, "Object2D.DoubleSidedCulling.BlendNormal");
        objects2D_.emplace_back(std::move(spriteObj));
    }

    initialized_ = true;
}

void TestScene::OnUpdate() {
    InitializeTestObjects();

    for (auto &obj : objects3D_) {
        if (!obj) continue;
        if (obj->GetName() == "Camera3D") continue;
        if (obj->GetName() == "DirectionalLight") continue;

        if (auto *transformComp = obj->GetComponent3D<Transform3D>()) {
            Vector3 rotate = transformComp->GetRotate();
            rotate.y += 0.01f;
            transformComp->SetRotate(rotate);
        }
    }

    // ScreenBuffer用スプライトの位置、サイズ更新
    for (auto &obj : objects2D_) {
        if (!obj) continue;
        if (obj->GetName() != "OffscreenBufferDisplaySprite") continue;
        if (auto *transformComp = obj->GetComponent2D<Transform2D>()) {
            if (auto *window = Window::GetWindow("Overlay Window")) {
                const float w = static_cast<float>(window->GetClientWidth());
                const float h = static_cast<float>(window->GetClientHeight());
                transformComp->SetTranslate(Vector2(w * 0.25f, h * 0.25f));
                transformComp->SetScale(Vector2(w * 0.5f, h * -0.5f));
            }
        }
    }
}

} // namespace KashipanEngine
