#include "Scenes/TestScene.h"
#include "Scenes/Components/ScreenBufferKeepRatio.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"
#include "Scene/Components/ShadowMapCameraSync.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);
    shadowMapBuffer_ = ShadowMapBuffer::Create(2048, 2048);

    auto* window = Window::GetWindow("Main Window");

    // 2D Camera (window)
    {
        auto obj = std::make_unique<Camera2D>();
        obj->SetName("Camera2D_Window");
        if (window) {
            obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        screenCamera2D_ = obj.get();
        AddObject2D(std::move(obj));
    }

    // 2D Camera (screenBuffer_)
    {
        auto obj = std::make_unique<Camera2D>();
        obj->SetName("Camera2D_ScreenBuffer");
        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object2D.DoubleSidedCulling.BlendNormal");
            const float w = static_cast<float>(screenBuffer_->GetWidth());
            const float h = static_cast<float>(screenBuffer_->GetHeight());
            obj->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        AddObject2D(std::move(obj));
    }

    // 3D Main Camera (screenBuffer_)
    {
        auto obj = std::make_unique<Camera3D>();
        obj->SetName("Camera3D_Main(ScreenBuffer)");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 24.0f, -36.0f));
            tr->SetRotate(Vector3(M_PI * (30.0f / 180.0f), 0.0f, 0.0f));
        }
        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            const float w = static_cast<float>(screenBuffer_->GetWidth());
            const float h = static_cast<float>(screenBuffer_->GetHeight());
            obj->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        obj->SetFovY(0.7f);
        mainCamera3D_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Light Camera (shadowMapBuffer_)
    {
        auto obj = std::make_unique<Camera3D>();
        obj->SetName("Camera3D_Light(ShadowMapBuffer)");
        obj->SetConstantBufferRequirementKeys({ "Vertex:gCamera" });
        obj->SetCameraType(Camera3D::CameraType::Orthographic);
        obj->SetOrthographicParams(-25.0f, 25.0f, 25.0f, -25.0f, 0.1f, 200.0f);
        if (shadowMapBuffer_) {
            obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
            const float w = static_cast<float>(shadowMapBuffer_->GetWidth());
            const float h = static_cast<float>(shadowMapBuffer_->GetHeight());
            obj->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        lightCamera3D_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Directional Light (screenBuffer_)
    {
        auto obj = std::make_unique<DirectionalLight>();
        obj->SetName("DirectionalLight");
        obj->SetEnabled(true);
        obj->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        obj->SetDirection(Vector3(1.8f, -2.0f, 1.2f));
        obj->SetIntensity(1.6f);
        light_ = obj.get();

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // Shadow Map Binder
    {
        auto obj = std::make_unique<ShadowMapBinder>();
        obj->SetName("ShadowMapBinder");
        obj->SetShadowMapBuffer(shadowMapBuffer_);
        const auto sampler = GetSceneVariableOr("ShadowSampler", SamplerManager::kInvalidHandle);
        obj->SetShadowSampler(sampler);
        obj->SetCamera3D(lightCamera3D_);
        shadowMapBinder_ = obj.get();
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    //==================================================
    // ↓ ここからゲームオブジェクト定義 ↓
    //==================================================

    // map (Box scaled)
    {
        for (int z = 0; z < kMapH; z++) {
            for (int x = 0; x < kMapW; x++) {

                auto obj = std::make_unique<Box>();
                obj->SetName("Map" ":x" + std::to_string(x) + ":z" + std::to_string(z));

                if (auto* tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(Vector3(2.0f * x, 0.0f, 2.0f * z));
                    tr->SetScale(Vector3(mapScaleMax_));
                }

                if (auto* mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetTexture(TextureManager::GetTextureFromFileName("uvChecker.png"));
                }

                if (screenBuffer_)     obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
                if (shadowMapBuffer_)  obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");

                // ここで “AddObject3D する前” にポインタ確保
                maps_[z][x] = obj.get();

                AddObject3D(std::move(obj));
            }
        }
    }

    // Player
    {
        auto modelData = ModelManager::GetModelDataFromFileName("Player.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Player");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 2.0f, 0.0f));
            tr->SetScale(Vector3(playerScaleMax_));
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        player_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Particles
    {
        ParticleMovement::SpawnBox spawn;
        spawn.min = Vector3(-16.0f, 0.0f, -16.0f);
        spawn.max = Vector3(16.0f, 16.0f, 16.0f);

        for (int i = 0; i < 32; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("ParticleBox_" + std::to_string(i));
            obj->RegisterComponent<ParticleMovement>(spawn, 0.5f, 10.0f, Vector3{0.5f, 0.5f, 0.5f});

            if (auto* mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");

            AddObject3D(std::move(obj));
        }
    }

    // ScreenBuffer -> Window
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("ScreenBufferSprite");
        if (screenBuffer_) {
            if (auto* mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(screenBuffer_);
            }
        }
        obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
        screenSprite_ = obj.get();
        AddObject2D(std::move(obj));
    }

    //==================================================
    // ↑ ここまでゲームオブジェクト定義 ↑
    //==================================================

    // BPMシステムの追加
    {
        auto comp = std::make_unique<BPMSystem>(bpm_); // BPM で初期化
        bpmSystem_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // Keep ratio
    {
        auto comp = std::make_unique<ScreenBufferKeepRatio>();
        comp->SetSprite(screenSprite_);
        comp->SetTargetSize(0.0f, 0.0f);
        if (screenBuffer_) {
            comp->SetSourceSize(static_cast<float>(screenBuffer_->GetWidth()), static_cast<float>(screenBuffer_->GetHeight()));
        }
        AddSceneComponent(std::move(comp));
    }

    // Shadow map camera sync (fit main camera view)
    {
        auto comp = std::make_unique<ShadowMapCameraSync>();
        comp->SetMainCamera(mainCamera3D_);
        comp->SetLightCamera(lightCamera3D_);
        comp->SetDirectionalLight(light_);
        comp->SetShadowMapBinder(shadowMapBinder_);
        comp->SetShadowMapBuffer(shadowMapBuffer_);
        AddSceneComponent(std::move(comp));
    }
}

TestScene::~TestScene() {
    ClearObjects2D();
    ClearObjects3D();

    if (screenBuffer_) {
        ScreenBuffer::DestroyNotify(screenBuffer_);
        screenBuffer_ = nullptr;
    }
    if (shadowMapBuffer_) {
        ShadowMapBuffer::DestroyNotify(shadowMapBuffer_);
        shadowMapBuffer_ = nullptr;
    }
}

void TestScene::OnUpdate() {
    // window resize
    if (screenCamera2D_ && screenSprite_) {
        if (auto* window = Window::GetWindow("Main Window")) {
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            screenCamera2D_->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            screenCamera2D_->SetViewportParams(0.0f, 0.0f, w, h);
        }
    }

    if (player_) {
        if (auto* tr = player_->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3(EaseInBack(playerScaleMin_, playerScaleMax_, bpmSystem_->GetBeatProgress())));
        }
    }

    {
        for (int z = 0; z < kMapH; z++) {
            for (int x = 0; x < kMapW; x++) {
                if (maps_[z][x]) {
                    if (auto* tr = maps_[z][x]->GetComponent3D<Transform3D>()) {
                        tr->SetScale(Vector3(EaseInBack(mapScaleMin_, mapScaleMax_, bpmSystem_->GetBeatProgress())));
                    }
                }
            }
        }
    }
}

} // namespace KashipanEngine
