#include "Scenes/TitleScene.h"

#include "Core/Window.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"

#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/2D/Sprite.h"

#include "Objects/Components/3D/Collision3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"

#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/3D/Billboard.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Model.h"

#include "Objects/Components/ParticleMovement.h"
#include "Objects/Components/PlayerMovement.h"

#include "Utilities/MathUtils.h"
#include "Assets/AudioManager.h"

#include "Objects/SystemObjects/ShadowMapBinder.h"
#include "Objects/SystemObjects/LightManager.h"
#include "Scene/Components/ShadowMapCameraSync.h"
#include "Objects/SystemObjects/VelocityBufferCameraBinder.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Scene/Components/ColliderComponent.h"

#include <memory>

namespace KashipanEngine {

namespace {
constexpr float kPi = 3.14159265358979323846f;

Vector3 ReflectPointAcrossY0(const Vector3 &p) {
    return Vector3{p.x, -p.y, p.z};
}

Vector3 ReflectDirectionAcrossY0(const Vector3 &v) {
    return Vector3{v.x, -v.y, v.z};
}
} // namespace

TitleScene::TitleScene()
    : SceneBase("TitleScene") {
    {
        const auto sound = AudioManager::GetSoundHandleFromFileName("titleBGM.mp3");
        bgmPlay_ = AudioManager::Play(sound, 1.0f, 0.0f, true);
    }

    auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // デフォルト変数を取得
    [[maybe_unused]] auto *defaultVariables = GetSceneComponent<SceneDefaultVariables>();
    [[maybe_unused]] auto *screenBuffer3D = defaultVariables ? defaultVariables->GetScreenBuffer3D() : nullptr;
    [[maybe_unused]] auto *screenBuffer2D = defaultVariables ? defaultVariables->GetScreenBuffer2D() : nullptr;
    [[maybe_unused]] auto *shadowMapBuffer = defaultVariables ? defaultVariables->GetShadowMapBuffer() : nullptr;
    [[maybe_unused]] auto *lightManager = defaultVariables ? defaultVariables->GetLightManager() : nullptr;
    [[maybe_unused]] auto *mainCamera3D = defaultVariables ? defaultVariables->GetMainCamera3D() : nullptr;
    [[maybe_unused]] auto *lightCamera3D = defaultVariables ? defaultVariables->GetLightCamera3D() : nullptr;
    [[maybe_unused]] auto *directionalLight = defaultVariables ? defaultVariables->GetDirectionalLight() : nullptr;
    [[maybe_unused]] auto *shadowMapBinder = defaultVariables ? defaultVariables->GetShadowMapBinder() : nullptr;
    [[maybe_unused]] auto *shadowMapCameraSync = defaultVariables ? defaultVariables->GetShadowMapCameraSync() : nullptr;
    [[maybe_unused]] auto *screenSprite = defaultVariables ? defaultVariables->GetScreenBuffer2DSprite() : nullptr;
    [[maybe_unused]] auto *colliderComp = defaultVariables ? defaultVariables->GetColliderComp() : nullptr;

    if (screenBuffer3D) {
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 1.0f;
        bp.softKnee = 0.25f;
        bp.intensity = 0.5f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        screenBuffer3D->AttachToRenderer("ScreenBuffer_TitleScene");
    }

    // カメラの初期値設定
    if (mainCamera3D) {
        if (auto *tr = mainCamera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 8.0f, -10.0f));
            tr->SetRotate(Vector3(kPi * (30.0f / 180.0f), 0.0f, 0.0f));
        }
        if (screenBuffer3D) {
            const float w = static_cast<float>(screenBuffer3D->GetWidth());
            const float h = static_cast<float>(screenBuffer3D->GetHeight());
            mainCamera3D->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
            mainCamera3D->SetViewportParams(0.0f, 0.0f, w, h);
        }
        mainCamera3D->SetFovY(0.7f);
    }

    // シャドウマップカメラ同期コンポーネントの設定
    if (shadowMapCameraSync) {
        shadowMapCameraSync->SetShadowNear(0.1f);
        shadowMapCameraSync->SetShadowFar(16.0f);
    }

    // 平行光源の初期値設定
    if (directionalLight) {
        directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        directionalLight->SetDirection(Vector3(4.0f, -2.0f, 3.0f));
        directionalLight->SetIntensity(1.0f);
    }

    //==================================================
    // ↓ ここからゲームオブジェクト定義 ↓
    //==================================================

    // Title logo Plane3D
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("TitleLogoPlane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 4.0f, 0.0f));
            tr->SetScale(Vector3(8.0f, 2.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("titleLogo.png"));
            mat->SetEnableLighting(false);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Player Sphere
    {
        auto obj = std::make_unique<Sphere>();
        obj->SetName("Player");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 1.0f, 0.0f));
            tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
        }
        ColliderInfo3D info{};
        info.shape = Math::Sphere{Vector3{0.0f, 0.0f, 0.0f}, 0.5f};
        obj->RegisterComponent<Collision3D>(colliderComp->GetCollider(), info);
        obj->RegisterComponent<PlayerMovement>(GetInputCommand());

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Start Plane
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("StartPlane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.8f, -2.0f));
            tr->SetScale(Vector3(2.0f, 1.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("startText.png"));
            mat->SetEnableLighting(false);
        }
        ColliderInfo3D info{};
        info.shape = Math::AABB{Vector3{-1.0f, -0.5f, -0.01f}, Vector3{1.0f, 0.5f, 0.01f}};
        info.onCollisionEnter = [this](const HitInfo3D &hitInfo) {
            if (!hitInfo.isHit) return;
            if (auto *mat = hitInfo.selfObject->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
            }
        };
        info.onCollisionStay = [this](const HitInfo3D &hitInfo) {
            if (!hitInfo.isHit) return;
            auto r = GetInputCommand()->Evaluate("Submit");
            if (r.Triggered()) {
                SetNextSceneName("GameScene");
                auto soundHandle = AudioManager::GetSoundHandleFromFileName("submit.mp3");
                AudioManager::Play(soundHandle, 1.0f, 0.0f, false);

                auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>();
                if (sceneChangeOut) {
                    sceneChangeOut->Play();
                }
            }
        };
        info.onCollisionExit = [this](const HitInfo3D &hitInfo) {
            if (auto *mat = hitInfo.selfObject->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }
        };
        obj->RegisterComponent<Collision3D>(colliderComp->GetCollider(), info);

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Floor Plane
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetName("FloorPlane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
            tr->SetRotate(Vector3(kPi * 0.5f, 0.0f, 0.0f));
            tr->SetScale(Vector3(1024.0f, 1024.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Particle Billboards
    {
        constexpr std::uint32_t kParticleCount = 128;
        for (std::uint32_t i = 0; i < kParticleCount; ++i) {
            auto obj = std::make_unique<Billboard>();
            obj->SetName(std::string("ParticleBillboard_") + std::to_string(i));
            obj->SetCamera(mainCamera3D);
            obj->SetFacingMode(Billboard::FacingMode::LookAtCamera);
            obj->RegisterComponent<ParticleMovement>(
                ParticleMovement::SpawnBox{
                    Vector3{-10.0f, 0.0f, -10.0f},
                    Vector3{10.0f, 10.0f, 10.0f}},
                0.5f,
                5.0f,
                Vector3{0.2f, 0.2f, 0.2f});

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetTexture(whiteTex);
            }
            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
            AddObject3D(std::move(obj));
        }
    }

    // 天球
    {
        auto modelData = ModelManager::GetModelDataFromFileName("skySphere.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("SkySphere");
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableShadowMapProjection(false);
            mat->SetTexture(whiteTex);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    auto *sceneChangeIn = GetSceneComponent<SceneChangeIn>();
    if (sceneChangeIn) {
        sceneChangeIn->Play();
    }
}

TitleScene::~TitleScene() {
    if (bgmPlay_ != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(bgmPlay_);
        bgmPlay_ = AudioManager::kInvalidPlayHandle;
    }
}

void TitleScene::OnUpdate() {
    // SceneChangeOut 完了で次シーンへ
    if (!GetNextSceneName().empty()) {
        if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
            if (sceneChangeOut->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
