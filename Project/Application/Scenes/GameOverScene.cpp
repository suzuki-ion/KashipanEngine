#include "Scenes/GameOverScene.h"

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
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);

    auto colliderComp = std::make_unique<ColliderComponent>();
    auto *collider = colliderComp->GetCollider();
    AddSceneComponent(std::move(colliderComp));

    auto *window = Window::GetWindow("Main Window");
    const auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // 2D Camera (window)
    {
        auto obj = std::make_unique<Camera2D>();
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
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 8.0f, -10.0f));
            tr->SetRotate(Vector3(kPi * (30.0f / 180.0f), 0.0f, 0.0f));
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

    // Directional Light
    {
        auto obj = std::make_unique<DirectionalLight>();
        if (auto *light = obj.get()) {
            light->SetEnabled(true);
            light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            light->SetDirection(Vector3(4.0f, -2.0f, 1.0f));
            light->SetIntensity(1.6f);
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
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
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
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
        obj->RegisterComponent<Collision3D>(collider, info);
        obj->RegisterComponent<PlayerMovement>(GetInputCommand());
        player_ = obj.get();
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));

        // プレイヤーの移動範囲を制限する
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
        obj->RegisterComponent<Collision3D>(collider, info);

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
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
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // ScreenBuffer display sprite
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("ScreenBufferSprite");
        if (screenBuffer_) {
            if (auto *mat = obj->GetComponent2D<Material2D>()) {
                mat->SetTexture(screenBuffer_);
            }
        }
        obj->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
        screenSprite_ = obj.get();
        AddObject2D(std::move(obj));
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
    ClearObjects2D();
    ClearObjects3D();
}

void GameOverScene::OnUpdate() {
    if (screenCamera2D_ && screenSprite_) {
        if (auto *window = Window::GetWindow("Main Window")) {
            const float w = static_cast<float>(window->GetClientWidth());
            const float h = static_cast<float>(window->GetClientHeight());
            screenCamera2D_->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
            screenCamera2D_->SetViewportParams(0.0f, 0.0f, w, h);

            if (auto *tr = screenSprite_->GetComponent2D<Transform2D>()) {
                float drawW = w;
                float drawH = h;
                if (screenBuffer_) {
                    const float srcW = static_cast<float>(screenBuffer_->GetWidth());
                    const float srcH = static_cast<float>(screenBuffer_->GetHeight());
                    if (srcW > 0.0f && srcH > 0.0f && w > 0.0f && h > 0.0f) {
                        const float srcAspect = srcW / srcH;
                        const float dstAspect = w / h;
                        if (dstAspect > srcAspect) {
                            drawH = h;
                            drawW = drawH * srcAspect;
                        } else {
                            drawW = w;
                            drawH = drawW / srcAspect;
                        }
                    }
                }
                tr->SetTranslate(Vector2{w * 0.5f, h * 0.5f});
                tr->SetScale(Vector2{drawW, -drawH});
            }
        }
    }

    if (!GetNextSceneName().empty()) {
        if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
            if (sceneChangeOut->IsFinished()) {
                ChangeToNextScene();
            }
        }
    }
}

} // namespace KashipanEngine
