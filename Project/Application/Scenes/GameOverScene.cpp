#include "Scenes/GameOverScene.h"

#include "Core/Window.h"
#include "Scene/Components/ColliderComponent.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/SystemObjects/LightManager.h"

#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/2D/Sprite.h"

#include "Objects/Components/3D/Collision3D.h"
#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"

#include "Objects/Components/PlayerMovement.h"

#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Model.h"

#include <memory>

namespace KashipanEngine {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

GameOverScene::GameOverScene()
    : SceneBase("GameOverScene") {
    {
        const auto sound = AudioManager::GetSoundHandleFromFileName("gameOverBGM.mp3");
        bgmPlay_ = AudioManager::Play(sound, 1.0f, 0.0f, true);
    }

    auto *defaultVariables = GetSceneComponent<SceneDefaultVariables>();
    auto *screenBuffer3D = defaultVariables ? defaultVariables->GetScreenBuffer3D() : nullptr;
    auto *mainCamera3D = defaultVariables ? defaultVariables->GetMainCamera3D() : nullptr;
    auto *shadowMapCameraSync = defaultVariables ? defaultVariables->GetShadowMapCameraSync() : nullptr;
    auto *shadowMapBuffer = defaultVariables ? defaultVariables->GetShadowMapBuffer() : nullptr;
    auto *directionalLight = defaultVariables ? defaultVariables->GetDirectionalLight() : nullptr;
    auto *colliderComp = defaultVariables ? defaultVariables->GetColliderComp() : nullptr;

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
        directionalLight->SetDirection(Vector3(4.0f, -2.0f, 1.0f));
        directionalLight->SetIntensity(1.6f);
    }

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
        screenBuffer3D->AttachToRenderer("ScreenBuffer3D_GameOverScene");
    }

    const auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // Ensure directional light exists (use default)
    if (directionalLight) {
        directionalLight->SetEnabled(true);
        directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        directionalLight->SetDirection(Vector3(4.0f, -2.0f, 1.0f));
        directionalLight->SetIntensity(1.6f);
    }

    // GameOver logo Plane3D
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("GameOverLogoPlane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 4.0f, 0.0f));
            tr->SetScale(Vector3(8.0f, 2.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("gameOverLogo.png"));
            mat->SetEnableLighting(false);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    // Player Sphere (TitleScene same)
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
        player_ = obj.get();
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));

        if (player_) {
            if (auto *pm = player_->GetComponent3D<PlayerMovement>()) {
                pm->SetBoundsXZ(playerMoveMin_, playerMoveMax_);
            }
        }
    }

    auto makeMenuPlane = [&](const std::string &name, const Vector3 &pos, const std::string &tex, const std::string &nextScene) {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName(name);
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(pos);
            tr->SetScale(Vector3(2.0f, 1.0f, 1.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName(tex));
            mat->SetEnableLighting(false);
        }

        ColliderInfo3D info{};
        info.shape = Math::AABB{Vector3{-1.0f, -0.5f, -0.01f}, Vector3{1.0f, 0.5f, 0.01f}};
        info.onCollisionEnter = [](const HitInfo3D &hitInfo) {
            if (!hitInfo.isHit) return;
            if (auto *mat = hitInfo.selfObject->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f));
            }
        };
        info.onCollisionStay = [this, nextScene](const HitInfo3D &hitInfo) {
            if (!hitInfo.isHit) return;
            auto r = GetInputCommand()->Evaluate("Submit");
            if (r.Triggered()) {
                SetNextSceneName(nextScene);
                auto soundHandle = AudioManager::GetSoundHandleFromFileName("submit.mp3");
                AudioManager::Play(soundHandle, 1.0f, 0.0f, false);

                if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
                    sceneChangeOut->Play();
                }
            }
        };
        info.onCollisionExit = [](const HitInfo3D &hitInfo) {
            if (auto *mat = hitInfo.selfObject->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }
        };
        obj->RegisterComponent<Collision3D>(colliderComp->GetCollider(), info);

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    };

    makeMenuPlane("RetryPlane", Vector3(-2.5f, 0.8f, -2.0f), "retryText.png", "GameScene");
    makeMenuPlane("BackPlane", Vector3(2.5f, 0.8f, -2.0f), "backText.png", "TitleScene");

    // 天球
    {
        auto modelData = ModelManager::GetModelDataFromFileName("skySphere.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("SkySphere");
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetTexture(whiteTex);
            mat->SetColor(Vector4(0.0f, 0.0f, 0.0f, 1.0f));
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        AddObject3D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *sceneChangeIn = GetSceneComponent<SceneChangeIn>()) {
        sceneChangeIn->Play();
    }
}

GameOverScene::~GameOverScene() {
    if (bgmPlay_ != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(bgmPlay_);
        bgmPlay_ = AudioManager::kInvalidPlayHandle;
    }
}

void GameOverScene::OnUpdate() {
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
