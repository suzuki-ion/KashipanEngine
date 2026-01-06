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
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);
    mirrorBuffer_ = ScreenBuffer::Create(1024, 1024);

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

    // 3D Mirror Camera (mirrorBuffer_)
    {
        auto obj = std::make_unique<Camera3D>();
        if (mirrorBuffer_) {
            obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");
            const float w = static_cast<float>(mirrorBuffer_->GetWidth());
            const float h = static_cast<float>(mirrorBuffer_->GetHeight());
            obj->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
            obj->SetViewportParams(0.0f, 0.0f, w, h);
        }
        obj->SetFovY(0.7f);
        mirrorCamera3D_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Directional Light (両方に描く)
    {
        auto obj = std::make_unique<DirectionalLight>();
        if (auto *light = obj.get()) {
            light->SetEnabled(true);
            light->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            light->SetDirection(Vector3(4.0f, -2.0f, 1.0f));
            light->SetIntensity(1.6f);
        }
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (mirrorBuffer_) obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // Title logo Plane3D (両方に描く)
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
        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (mirrorBuffer_) obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // Player Sphere (両方に描く)
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

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (mirrorBuffer_) obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");

        player_ = obj.get();
        AddObject3D(std::move(obj));

        // プレイヤーの移動範囲を制限する
        if (player_) {
            if (auto *pm = player_->GetComponent3D<PlayerMovement>()) {
                pm->SetBoundsXZ(playerMoveMin_, playerMoveMax_);
            }
        }
    }

    // Start Plane (両方に描く)
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
        obj->RegisterComponent<Collision3D>(collider, info);

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (mirrorBuffer_) obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // Floor Plane (鏡) : 通常描画には含める / 鏡用バッファには含めない
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
        ColliderInfo3D info{};
        info.shape = Math::Plane{Vector3{0.0f, 1.0f, 0.0f}, 0.0f};
        obj->RegisterComponent<Collision3D>(collider, info);

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");

        floorPlane_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Particle Billboards (両方に描く)
    {
        constexpr std::uint32_t kParticleCount = 128;
        for (std::uint32_t i = 0; i < kParticleCount; ++i) {
            auto obj = std::make_unique<Billboard>();
            obj->SetName(std::string("ParticleBillboard_") + std::to_string(i));
            obj->SetCamera(mainCamera3D_);
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
            if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            if (mirrorBuffer_) obj->AttachToRenderer(mirrorBuffer_, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(obj));
        }
    }

    // 天球
    {
        auto modelData = ModelManager::GetModelDataFromFileName("skySphere.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("SkySphere");
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
        }
        obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }
    // FloorPlane に鏡用テクスチャを貼る（UV左右反転で鏡っぽく）
    /*if (floorPlane_ && mirrorBuffer_) {
        if (auto *mat = floorPlane_->GetComponent3D<Material3D>()) {
            mat->SetTexture(mirrorBuffer_);

            Material3D::UVTransform uv{};
            uv.scale = Vector3{1.0f, 1.0f, 1.0f};
            uv.translate = Vector3{0.0f, 0.0f, 0.0f};
            mat->SetUVTransform(uv);
        }
    }*/

    // ScreenBuffer用スプライト（最終表示）
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
    ClearObjects2D();
    ClearObjects3D();
}

void TitleScene::OnUpdate() {
    // 反射カメラ更新（XZ平面=Y=0で反射）
    if (mainCamera3D_ && mirrorCamera3D_) {
        auto *srcTr = mainCamera3D_->GetComponent3D<Transform3D>();
        auto *dstTr = mirrorCamera3D_->GetComponent3D<Transform3D>();
        if (srcTr && dstTr) {
            const Vector3 srcPos = srcTr->GetTranslate();
            const Vector3 srcRot = srcTr->GetRotate();

            dstTr->SetTranslate(ReflectPointAcrossY0(srcPos));

            // 簡易：pitchだけ反転（XZ床反射の最低限の見た目）
            Vector3 dstRot = srcRot;
            dstRot.x = -dstRot.x;
            dstTr->SetRotate(dstRot);
        }
    }

    // ScreenBuffer のサイズをウィンドウサイズに合わせる
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

                        // Fit (contain): アスペクト比維持でウィンドウ内に収める（余白が出る）
                        if (dstAspect > srcAspect) {
                            // ウィンドウが横長 -> 高さ基準
                            drawH = h;
                            drawW = drawH * srcAspect;
                        } else {
                            // ウィンドウが縦長 -> 幅基準
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
