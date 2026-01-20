#include "Scenes/GameScene.h"

#include "Core/Window.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Scenes/Components/CameraController.h"
#include "Scenes/Components/GameProgressController.h"
#include "Scenes/Components/AttackGearWallForward.h"
#include "Scenes/Components/AttackGearWallLeftSide.h"
#include "Scenes/Components/AttackGearWallRightSide.h"
#include "Scenes/Components/AttackGearCircularOutside.h"
#include "Scenes/Components/AttackGearCircularInside.h"
#include "Scenes/Components/BreakParticleGenerator.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Scenes/Components/ResultUI.h"
#include "Scenes/Components/JustAvoidParticle.h"
#include "Scenes/Components/GameIntroLogoAnimation.h"
#include "Scene/Components/ColliderComponent.h"

#include "Objects/SystemObjects/Camera2D.h"
#include "Objects/SystemObjects/Camera3D.h"
#include "Objects/SystemObjects/DirectionalLight.h"
#include "Objects/SystemObjects/LightManager.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"

#include "Scene/Components/ShadowMapCameraSync.h"

#include "Objects/Components/2D/Transform2D.h"
#include "Objects/Components/2D/Material2D.h"
#include "Objects/GameObjects/2D/Sprite.h"

#include "Objects/Components/3D/Transform3D.h"
#include "Objects/Components/3D/Material3D.h"
#include "Objects/Components/3D/Collision3D.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/Components/PlayerMovement.h"
#include "Objects/Components/AlwaysRotate.h"
#include "Objects/Components/Health.h"
#include "Objects/MathObjects/3D/Sphere.h"

#include "Objects/GameObjects/3D/Plane3D.h"
#include "Objects/GameObjects/3D/Billboard.h"
#include "Objects/GameObjects/3D/Sphere.h"
#include "Objects/GameObjects/3D/Model.h"

#include "Assets/AudioManager.h"

#include <cmath>
#include <random>

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {

    {
        const auto sound = AudioManager::GetSoundHandleFromFileName("gameBGM.mp3");
        bgmPlay_ = AudioManager::Play(sound, 1.0f, 0.0f, true);
    }

    std::vector<Object3DBase *> rotatingPlanes;

    // SceneDefaultVariables からポインタを取得して使用
    [[maybe_unused]] auto *defaultVariables = GetSceneComponent<SceneDefaultVariables>();
    [[maybe_unused]] auto *screenBuffer3D = defaultVariables ? defaultVariables->GetScreenBuffer3D() : nullptr;
    [[maybe_unused]] auto *screenBuffer2D = defaultVariables ? defaultVariables->GetScreenBuffer2D() : nullptr;
    [[maybe_unused]] auto *mainCamera3D = defaultVariables ? defaultVariables->GetMainCamera3D() : nullptr;
    [[maybe_unused]] auto *shadowMapBuffer = defaultVariables ? defaultVariables->GetShadowMapBuffer() : nullptr;
    [[maybe_unused]] auto *lightManager = defaultVariables ? defaultVariables->GetLightManager() : nullptr;
    [[maybe_unused]] auto *lightCamera3D = defaultVariables ? defaultVariables->GetLightCamera3D() : nullptr;
    [[maybe_unused]] auto *directionalLight = defaultVariables ? defaultVariables->GetDirectionalLight() : nullptr;
    [[maybe_unused]] auto *shadowMapBinder = defaultVariables ? defaultVariables->GetShadowMapBinder() : nullptr;
    [[maybe_unused]] auto *shadowCameraSync = defaultVariables ? defaultVariables->GetShadowMapCameraSync() : nullptr;
    [[maybe_unused]] auto *screen2DSprite = defaultVariables ? defaultVariables->GetScreenBuffer2DSprite() : nullptr;
    [[maybe_unused]] auto *screen3DSprite = defaultVariables ? defaultVariables->GetScreenBuffer3DSprite() : nullptr;

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

        screenBuffer3D->AttachToRenderer("ScreenBuffer3D_GameScene");
    }

    const auto whiteTex = TextureManager::GetTextureFromFileName("white1x1.png");

    // デフォルトからメインカメラを設定
    if (mainCamera3D) {
        if (auto *tr = mainCamera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 5.0f, -14.0f));
            tr->SetRotate(Vector3(M_PI * (15.0f / 180.0f), 0.0f, 0.0f));
        }
        if (screenBuffer3D) {
            const float w = static_cast<float>(screenBuffer3D->GetWidth());
            const float h = static_cast<float>(screenBuffer3D->GetHeight());
            mainCamera3D->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        }
        mainCamera3D->SetFovY(0.7f);

        auto camCtrl = std::make_unique<CameraController>(mainCamera3D);
        camCtrl->SetTargetRotate(Vector3(M_PI * (15.0f / 180.0f), 0.0f, 0.0f));
        camCtrl->SetTargetFovY(1.0f);
        camCtrl->SetLerpFactor(0.2f);

        cameraController_ = camCtrl.get();
        AddSceneComponent(std::move(camCtrl));
    }

    // デフォルトから平行光源を設定
    if (directionalLight) {
        directionalLight->SetEnabled(true);
        directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        directionalLight->SetDirection(Vector3(4.0f, -2.0f, 3.0f));
        directionalLight->SetIntensity(1.0f);
    }

    // Mover Sphere（移動の親となるオブジェクト、レンダラーへのアタッチはしない）
    {
        auto obj = std::make_unique<Sphere>();
        obj->SetName("MoverSphere");
        mover_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Floor Plane (XZ)
    {
        auto obj = std::make_unique<Plane3D>();
        obj->SetUniqueBatchKey();
        obj->SetName("FloorPlane");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
            tr->SetRotate(Vector3(M_PI * 0.5f, 0.0f, 0.0f));
            tr->SetScale(Vector3(10.0f, 10.0f, 1.0f));
            if (mover_) {
                if (auto *moverTr = mover_->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(moverTr);
                }
            }
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
        }

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        floorPlane_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Player Sphere
    {
        auto obj = std::make_unique<Sphere>();
        obj->SetName("Player");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.5f, 0.0f));
            tr->SetScale(Vector3(1.0f, 1.0f, 1.0f));
            if (mover_) {
                if (auto *moverTr = mover_->GetComponent3D<Transform3D>()) {
                    tr->SetParentTransform(moverTr);
                }
            }
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetTexture(whiteTex);
        }
        obj->RegisterComponent<PlayerMovement>(GetInputCommand());

        int health = 15;
#if defined(DEBUG_BUILD) || defined(DEVELOPMENT_BUILD)
        health = 15;
#endif
        obj->RegisterComponent<Health>(health);

        if (auto *collComp = GetSceneComponent<ColliderComponent>()) {
            ColliderInfo3D info;
            Math::Sphere sp;
            sp.center = Vector3{0.0f, 0.0f, 0.0f};
            sp.radius = 0.5f;
            info.shape = sp;

            info.onCollisionEnter = [](const HitInfo3D &hit) {
                if (!hit.selfObject || !hit.otherObject) return;
                auto *health = hit.selfObject->GetComponent3D<Health>();
                if (!health) return;
                auto *pm = hit.selfObject->GetComponent3D<PlayerMovement>();
                if (pm && pm->IsDashing()) return;
                health->Damage(1);
            };

            info.onCollisionStay = [this](const HitInfo3D &hit) {
                if (!hit.selfObject || !hit.otherObject) return;

                if (hit.otherObject->HasComponents3D("MovementController") == 0) return;

                auto *health = hit.selfObject->GetComponent3D<Health>();
                if (!health) return;

                if (auto *pm = hit.selfObject->GetComponent3D<PlayerMovement>()) {
                    if (pm->IsJustDodging() && !health->WasDamagedThisCooldown()) {
                        if (!justDodgeCountedThisDash_) {
                            ++justDodgeCount_;
                            justDodgeCountedThisDash_ = true;

                            auto *playerTr = player_->GetComponent3D<Transform3D>();
                            auto *playerMovement = player_->GetComponent3D<PlayerMovement>();
                            if (playerTr && playerMovement) {
                                const Vector3 p = playerTr->GetTranslate();
                                const Vector3 dashDir = playerMovement->GetVelocity().Normalize() * -1.0f;

                                if (auto *jp = GetSceneComponent<JustAvoidParticle>()) {
                                    jp->Spawn(p, dashDir);
                                }
                            }

                            auto soundHandle = AudioManager::GetSoundHandleFromFileName("avoidJust.mp3");
                            AudioManager::Play(soundHandle, 1.0f, 0.0f, false);
                        }
                        return;
                    }

                    if (pm->IsDashing()) return;
                }
            };

            obj->RegisterComponent<Collision3D>(collComp->GetCollider(), info);
        }

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        player_ = obj.get();

        AddObject3D(std::move(obj));

        if (cameraController_ && player_) {
            cameraController_->SetFollowTarget(player_);
            cameraController_->RecalculateOffsetFromCurrentCamera();
        }
    }

    // Rotating Planes
    {
        std::mt19937 rng{12345u};
        std::uniform_real_distribution<float> angleDist(0.0f, M_PI * 2.0f);
        std::uniform_real_distribution<float> scaleDist(6.0f, 10.0f);
        std::uniform_real_distribution<float> radiusDist(10.0f, 32.0f);

        constexpr std::uint32_t kCount = 512;
        for (std::uint32_t i = 0; i < kCount; ++i) {
            auto obj = std::make_unique<Plane3D>();
            obj->SetName(std::string("RotatingPlane_") + std::to_string(i));

            const float theta = angleDist(rng);
            const float r = radiusDist(rng);

            float x = std::cos(theta) * r;
            float y = std::sin(theta) * r;

            const float z = static_cast<float>(i) * 1.0f;
            const float rz = angleDist(rng);
            float scale = scaleDist(rng);

            if (i != 0 && (i % 64) == 0) {
                x = 0.0f;
                y = 0.0f;
                scale = 64.0f;
            }

            y += -10000.0f;

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(Vector3(x, y, z));
                tr->SetRotate(Vector3(0.0f, 0.0f, rz));
                tr->SetScale(Vector3(scale, scale, 1.0f));
            }
            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetTexture(TextureManager::GetTextureFromFileName("gears.png"));
                Material3D::UVTransform uvTrans;
                uvTrans.scale = Vector3(0.25f, 1.0f, 1.0f);
                uvTrans.translate = Vector3(static_cast<float>(i % 4) * 0.25f, 0.0f, 0.0f);
                mat->SetUVTransform(uvTrans);
            }

            obj->RegisterComponent<AlwaysRotate>(Vector3{0.0f, 0.0f, -1.0f});

            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
            rotatingPlanes.push_back(obj.get());
            AddObject3D(std::move(obj));
        }
    }

    // Particle Billboards
    {
        particleBillboards_.clear();
        constexpr std::uint32_t kParticleCount = 256;
        for (std::uint32_t i = 0; i < kParticleCount; ++i) {
            auto obj = std::make_unique<Billboard>();
            obj->SetName(std::string("ParticleBillboard_") + std::to_string(i));
            obj->SetCamera(mainCamera3D);
            obj->SetFacingMode(Billboard::FacingMode::LookAtCamera);

            obj->RegisterComponent<ParticleMovement>(
                ParticleMovement::SpawnBox{
                    Vector3{-32.0f, 0.0f, -32.0f},
                    Vector3{32.0f, 10.0f, 32.0f}},
                0.5f,
                5.0f,
                Vector3{0.2f, 0.2f, 0.2f});

            if (auto *mat = obj->GetComponent3D<Material3D>()) {
                mat->SetTexture(whiteTex);
            }
            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
            particleBillboards_.push_back(obj.get());
            AddObject3D(std::move(obj));
        }
    }

    // Sky Sphere
    {
        auto modelData = ModelManager::GetModelDataFromFileName("skySphere.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("SkySphere");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3(128.0f, 128.0f, 128.0f));
        }
        if (auto *mat = obj->GetComponent3D<Material3D>()) {
            mat->SetEnableLighting(false);
            mat->SetTexture(whiteTex);
        }
        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        skySphere_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Avoid command sprite
    {
        auto obj = std::make_unique<Sprite>();
        obj->SetUniqueBatchKey();
        obj->SetName("AvoidCommandText");

        if (auto *mat = obj->GetComponent2D<Material2D>()) {
            mat->SetTexture(TextureManager::GetTextureFromFileName("avoidCommandText.png"));
        }
        if (auto *tr = obj->GetComponent2D<Transform2D>()) {
            tr->SetTranslate(Vector2{256.0f, 128.0f});
            tr->SetScale(Vector2{512.0f, 256.0f});
        }

        if (screenBuffer2D) obj->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
        AddObject2D(std::move(obj));
    }

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    // 攻撃ギア系コンポーネントを登録（mover を保持させる）
    AddSceneComponent(std::make_unique<AttackGearWallForward>(mover_, screenBuffer3D));
    AddSceneComponent(std::make_unique<AttackGearWallLeftSide>(mover_, screenBuffer3D));
    AddSceneComponent(std::make_unique<AttackGearWallRightSide>(mover_, screenBuffer3D));
    AddSceneComponent(std::make_unique<AttackGearCircularOutside>(mover_, screenBuffer3D));
    AddSceneComponent(std::make_unique<AttackGearCircularInside>(mover_, screenBuffer3D));

    // ゲーム開始ロゴ
    AddSceneComponent(std::make_unique<GameIntroLogoAnimation>(screenBuffer2D));

    // ブレイクパーティクル
    AddSceneComponent(std::make_unique<BreakParticleGenerator>(screenBuffer3D, mover_));

    // ジャスト回避パーティクル
    AddSceneComponent(std::make_unique<JustAvoidParticle>(screenBuffer3D, mainCamera3D, mover_));

    // プレイヤー体力 UI
    auto playerHealthUI = std::make_unique<PlayerHealthUI>(screenBuffer2D);
    if (player_) {
        playerHealthUI->SetHealth(player_->GetComponent3D<Health>());
    }
    AddSceneComponent(std::move(playerHealthUI));
    // リザルト UI
    AddSceneComponent(std::make_unique<ResultUI>(screenBuffer2D, player_->GetComponent3D<Health>()));

    // ゲーム進行管理 - use default screen sprite if available
    AddSceneComponent(std::make_unique<GameProgressController>(mainCamera3D, directionalLight, mover_, screen3DSprite, rotatingPlanes));

    if (auto *sceneChangeIn = GetSceneComponent<SceneChangeIn>()) {
        sceneChangeIn->Play();
    }
}

GameScene::~GameScene() {
    if (bgmPlay_ != AudioManager::kInvalidPlayHandle) {
        AudioManager::Stop(bgmPlay_);
        bgmPlay_ = AudioManager::kInvalidPlayHandle;
    }
}

void GameScene::OnUpdate() {
    if (auto *gp = GetSceneComponent<GameProgressController>()) {
        if (gp->IsFinished() && !prevGameProgressFinished_) {
            if (auto *resultUI = GetSceneComponent<ResultUI>()) {
                resultUI->ShowStart();
            }
        }
        prevGameProgressFinished_ = gp->IsFinished();
    }

    // ResultUI アニメーション終了後、Submit で TitleScene へ
    if (auto *gp = GetSceneComponent<GameProgressController>()) {
        if (gp->IsFinished()) {
            if (auto *resultUI = GetSceneComponent<ResultUI>()) {
                if (resultUI->IsAnimationFinished()) {
                    if (GetNextSceneName().empty() && GetInputCommand()) {
                        const auto submit = GetInputCommand()->Evaluate("Submit");
                        if (submit.Triggered()) {
                            SetNextSceneName("TitleScene");
                            if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
                                sceneChangeOut->Play();
                            }
                        }
                    }
                }
            }
        }
    }

    // ジャスト回避のカウント制御（1回の回避入力で多重加算しない）
    if (player_) {
        if (auto *pm = player_->GetComponent3D<PlayerMovement>()) {
            const bool justDodging = pm->IsJustDodging();
            if (!justDodging && prevJustDodging_) {
                justDodgeCountedThisDash_ = false;
            }
            prevJustDodging_ = justDodging;
        }
    }
    if (auto *ui = GetSceneComponent<ResultUI>()) {
        ui->SetJustDodgeCount(justDodgeCount_);
    }

    if (player_) {
        if (auto *health = player_->GetComponent3D<Health>()) {
            const bool damaged = health->WasDamagedThisCooldown();
            if (damaged && !prevDamagedThisCooldown_) {
                {
                    const auto se = AudioManager::GetSoundHandleFromFileName("damage.mp3");
                    if (se != AudioManager::kInvalidSoundHandle) {
                        damagePlay_ = AudioManager::Play(se, 1.0f, 0.0f, false);
                    }
                }

                if (auto *tr = player_->GetComponent3D<Transform3D>()) {
                    if (auto *bp = GetSceneComponent<BreakParticleGenerator>()) {
                        bp->Generate(tr->GetTranslate());
                    }
                    if (cameraController_) {
                        cameraController_->Shake(2.0f, 0.5f);
                    }
                }

                if (health->GetHp() <= 0) {
                    SetNextSceneName("GameOverScene");
                    if (auto *sceneChangeOut = GetSceneComponent<SceneChangeOut>()) {
                        sceneChangeOut->Play();
                    }
                }
            }
            prevDamagedThisCooldown_ = damaged;
        }
    }

    if (floorPlane_ && player_) {
        auto *floorTr = floorPlane_->GetComponent3D<Transform3D>();
        auto *playerTr = player_->GetComponent3D<Transform3D>();
        if (floorTr && playerTr) {
            const Vector3 floorScale = floorTr->GetScale();

            const Vector3 half{floorScale.x * 0.5f, 0.0f, floorScale.y * 0.5f};
            const Vector3 minB{ -half.x, 0.0f, -half.z };
            const Vector3 maxB{ half.x, 0.0f, half.z };

            if (auto *pm = player_->GetComponent3D<PlayerMovement>()) {
                pm->SetBoundsXZ(minB, maxB);
            }
        }
    }

    if (player_) {
        if (auto *playerTr = player_->GetComponent3D<Transform3D>()) {
            const Matrix4x4 playerWorldMat = playerTr->GetWorldMatrix();
            const Vector3 p = Vector3(
                playerWorldMat.m[3][0],
                playerWorldMat.m[3][1],
                playerWorldMat.m[3][2]);
            const Vector3 half{32.0f, 10.0f, 32.0f};

            ParticleMovement::SpawnBox box;
            box.min = p - Vector3{half.x, 0.0f, half.z};
            box.max = p + half;
            box.min.y = 0.0f;

            for (auto *bb : particleBillboards_) {
                if (!bb) continue;
                if (auto *pm = bb->GetComponent3D<ParticleMovement>()) {
                    pm->SetSpawnBox(box);
                }
            }

            if (skySphere_) {
                if (auto *skyTr = skySphere_->GetComponent3D<Transform3D>()) {
                    skyTr->SetTranslate(Vector3{ p.x, p.y, p.z });
                }
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

}  // namespace KashipanEngine