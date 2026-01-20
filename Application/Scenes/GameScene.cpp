#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Objects/Components/ParticleMovement.h"

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {
}

void GameScene::Initialize() {
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);
    shadowMapBuffer_ = ShadowMapBuffer::Create(16384, 16384);

    /*if (screenBuffer3D_) {
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.0025f;
        screenBuffer3D_->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));
        screenBuffer3D_->AttachToRenderer("ScreenBuffer_GameScene");
    }*/

    auto* window = Window::GetWindow("Main Window");
    [[maybe_unused]] const auto whiteTexture = TextureManager::GetTextureFromFileName("white1x1.png");
    [[maybe_unused]] const auto uvCheckerTexture = TextureManager::GetTextureFromFileName("uvChecker.png");

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

    // 2D Camera (screenBuffer3D_)
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

    // 3D Main Camera (screenBuffer3D_)
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

    // Directional Light (screenBuffer3D_)
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
        obj->SetCamera3D(lightCamera3D_);
        shadowMapBinder_ = obj.get();
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    //==================================================
    // ↓ ここからゲームオブジェクト定義 ↓
    //==================================================

    // Floor (Box scaled)
    {
        auto obj = std::make_unique<Box>();
        obj->SetName("Floor");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, -0.5f, 0.0f));
            tr->SetScale(Vector3(64.0f, 1.0f, 64.0f));
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        floor_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Sphere
    {
        auto obj = std::make_unique<Sphere>(16, 32);
        obj->SetName("ShadowTestSphere");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 1.5f, 0.0f));
            tr->SetScale(Vector3(2.0f, 2.0f, 2.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(uvCheckerTexture);
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        sphere_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Particles
    {
        ParticleMovement::SpawnBox spawn;
        spawn.min = Vector3(-32.0f, 0.0f, -32.0f);
        spawn.max = Vector3(32.0f, 32.0f, 32.0f);

        for (int i = 0; i < 256; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("ParticleBox_" + std::to_string(i));
            obj->RegisterComponent<ParticleMovement>(spawn, 0.5f, 10.0f, Vector3{ 1.0f, 1.0f, 1.0f });

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

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }
}

GameScene::~GameScene() {
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

void GameScene::OnUpdate() {
    // window resize
    if (screenCamera2D_ && screenSprite_) {
        if (auto* window = Window::GetWindow("Main Window")) {
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            screenCamera2D_->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            screenCamera2D_->SetViewportParams(0.0f, 0.0f, w, h);
        }
    }

    // simple animation
    if (sphere_) {
        if (auto* tr = sphere_->GetComponent3D<Transform3D>()) {
            Vector3 r = tr->GetRotate();
            r.y += 0.01f;
            tr->SetRotate(r);
        }
    }

    if (auto *ic = GetInputCommand()) {
        if (ic->Evaluate("DebugSceneChange").Triggered()) {
            if (GetNextSceneName().empty()) {
                SetNextSceneName("ResultScene");
            }
            if (auto *out = GetSceneComponent<SceneChangeOut>()) {
                out->Play();
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *out = GetSceneComponent<SceneChangeOut>()) {
            if (out->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine