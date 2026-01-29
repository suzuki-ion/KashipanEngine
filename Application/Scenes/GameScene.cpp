#include "Scenes/GameScene.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Scenes/Components/PlayerHealthModelUI.h"
#include "Scenes/Components/ScoreUI.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Objects/Components/Player/BpmbSpawn.h"
#include "Objects/Components/Player/PlayerDieParticleManager.h"
#include "objects/Components/Health.h"
#include "Objects/SystemObjects/LightManager.h"
#include <cmath>

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    jsonManager_ = std::make_unique<JsonManager>();
    jsonManager_->SetBasePath("Assets/Application");

    jsonParticleManager_ = std::make_unique<JsonManager>();
    jsonParticleManager_->SetBasePath("Assets/Application");

    LoadObjectStateJson();
    LoadParticleStateJson();
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
    auto *mainWindow = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainWindow() : nullptr;
    auto *screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
    auto *screenBuffer3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer3D() : nullptr;
    auto *windowCamera = sceneDefaultVariables_ ? sceneDefaultVariables_->GetWindowCamera2D() : nullptr;
    auto *camera2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera2D() : nullptr;
    auto *camera3D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetMainCamera3D() : nullptr;
    auto *cameraLight = sceneDefaultVariables_ ? sceneDefaultVariables_->GetLightCamera3D() : nullptr;
    auto *shadowMapBuffer = sceneDefaultVariables_ ? sceneDefaultVariables_->GetShadowMapBuffer() : nullptr;
    auto *directionalLight = sceneDefaultVariables_ ? sceneDefaultVariables_->GetDirectionalLight() : nullptr;
    auto *colliderComp = sceneDefaultVariables_ ? sceneDefaultVariables_->GetColliderComp() : nullptr;
    ScreenBuffer *velocityBuffer = nullptr;

    if (screenBuffer3D) {
        /*ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));*/

        BloomEffect::Params bp{};
        bp.threshold = 0.5f;
        bp.softKnee = 0.25f;
        bp.intensity = 1.0f;
        bp.blurRadius = 1.0f;
        bp.iterations = 4;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));

        screenBuffer3D->AttachToRenderer("ScreenBuffer_TitleScene");
    }

    // 2D Camera (window)
    if (mainWindow && windowCamera) {
        const float w = static_cast<float>(mainWindow->GetClientWidth());
        const float h = static_cast<float>(mainWindow->GetClientHeight());
        windowCamera->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        windowCamera->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // 2D Camera (screenBuffer2D_)
    if (screenBuffer2D) {
        const float w = static_cast<float>(screenBuffer2D->GetWidth());
        const float h = static_cast<float>(screenBuffer2D->GetHeight());
        camera2D->SetOrthographicParams(0.0f, 0.0f, w, h, 0.0f, 1.0f);
        camera2D->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // 3D Main Camera (screenBuffer3D_)
    if (screenBuffer3D && camera3D) {
        if (auto *tr = camera3D->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(10.0f, 24.0f, -21.0f));
            tr->SetRotate(Vector3(0.6f, 0.0f, 0.0f));
        }
        const float w = static_cast<float>(screenBuffer3D->GetWidth());
        const float h = static_cast<float>(screenBuffer3D->GetHeight());
        camera3D->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        camera3D->SetViewportParams(0.0f, 0.0f, w, h);
        camera3D->SetFovY(0.7f);
    }

    // Light Camera (shadowMapBuffer_)
    if (shadowMapBuffer && cameraLight) {
        const float w = static_cast<float>(shadowMapBuffer->GetWidth());
        const float h = static_cast<float>(shadowMapBuffer->GetHeight());
        cameraLight->SetAspectRatio(h != 0.0f ? (w / h) : 1.0f);
        cameraLight->SetViewportParams(0.0f, 0.0f, w, h);
    }

    // Directional Light (screenBuffer3D_)
    directionalLight->SetEnabled(true);
    directionalLight->SetColor(Vector4(0.8f, 0.6f, 1.0f, 1.0f));
    directionalLight->SetDirection(Vector3(0.0f, -1.0f, 0.0f));
    directionalLight->SetIntensity(1.0f);

    //==================================================
    // ↓ ここからゲームオブジェクト定義 ↓
    //==================================================

    // BPMシステムの追加
    {
        auto comp = std::make_unique<BPMSystem>(static_cast<float>(bpm_)); // BPM で初期化
        bpmSystem_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    {
        auto comp = std::make_unique<CameraController>(camera3D);
        cameraController_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    {
		auto aBottom = ModelManager::GetModelDataFromFileName("aBomb.obj");
        auto obj = std::make_unique<Model>(aBottom);
        obj->SetName("A_Bomb");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(-13.33f, -0.21f, 30.12f));
            tr->SetParentTransform(camera3D->GetComponent3D<Transform3D>());
            tr->SetScale(Vector3(1.0f));
        }
        if (screenBuffer3D)  obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        aBomb_ = obj.get();
		AddObject3D(std::move(obj));

        auto haku = ModelManager::GetModelDataFromFileName("haku.obj");
        auto obj2 = std::make_unique<Model>(haku);
        obj2->SetName("haku");
        if (auto* tr = obj2->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(-10.83f, 0.88f, 27.3f));
            tr->SetParentTransform(camera3D->GetComponent3D<Transform3D>());
            tr->SetScale(Vector3(0.5f));
        }
        if (screenBuffer3D)  obj2->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        haku_ = obj2.get();
        AddObject3D(std::move(obj2));
    }

    // map (Box scaled)
    {
        for (int z = 0; z < kMapH; z++) {
            for (int x = 0; x < kMapW; x++) {

                auto modelData = ModelManager::GetModelDataFromFileName("block.obj");
                auto obj = std::make_unique<Model>(modelData);

                obj->SetName("Map" ":x" + std::to_string(x) + ":z" + std::to_string(z));

                obj->RegisterComponent<BPMScaling>(mapScaleMin_, mapScaleMax_, EaseType::EaseOutExpo);

                if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(Vector3(2.0f * x, 0.0f, 2.0f * z));
                    tr->SetScale(Vector3(mapScaleMax_));
                }

                if (auto *mt = obj->GetComponent3D<Material3D>()) {
                    mt->SetColor(Vector4{ 0.5f,0.5f,0.5f,1.0f });
                }

                if (screenBuffer3D)  obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
                if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
                if (velocityBuffer)  obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");

                // ここで "AddObject3D する前" にポインタ確保
                maps_[z][x] = obj.get();

                AddObject3D(std::move(obj));
            }
        }
    }

    {
        auto modelData = ModelManager::GetModelDataFromFileName("stage.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Stage");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(10.0f, -1.0f, 10.0f));
            tr->SetScale(Vector3(1.0f));
        }

        if (screenBuffer3D)  obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        //if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        if (velocityBuffer)  obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");
        stage_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // OneBeatEmitter の追加
    {
        emitterTranslate_[0] = Vector3(-2.0f, 0.5f, -2.0f);
        emitterTranslate_[1] = Vector3(-2.0f, 0.5f, 3.65f);
        emitterTranslate_[2] = Vector3(-2.0f, 0.5f, 10.0f);
        emitterTranslate_[3] = Vector3(-2.0f, 0.5f, 16.3f);
        emitterTranslate_[4] = Vector3(-2.0f, 0.5f, 22.0f);
        emitterTranslate_[5] = Vector3(22.0f, 0.5f, -2.0f);
        emitterTranslate_[6] = Vector3(22.0f, 0.5f, 3.65f);
        emitterTranslate_[7] = Vector3(22.0f, 0.5f, 10.0f);
        emitterTranslate_[8] = Vector3(22.0f, 0.5f, 16.3f);
        emitterTranslate_[9] = Vector3(22.0f, 0.5f, 22.0f);
        for (int i = 0; i < kOneBeatEmitterCount_; ++i) {
            auto obj = std::make_unique<Box>();
            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(emitterTranslate_[i]);
                tr->SetScale(Vector3(0.1f, 0.1f, 0.1f));
            }
            obj->SetName("A_OneBeatEmitterObj_" + std::to_string(i));
            oneBeatEmitterObj_[i] = obj.get();
            AddObject3D(std::move(obj));


            auto comp = std::make_unique<OneBeatEmitter>();
            comp->SetScreenBuffer(screenBuffer3D);
            comp->SetShadowMapBuffer(shadowMapBuffer);
            comp->SetBPMSystem(bpmSystem_);
            comp->SetEmitter(oneBeatEmitterObj_[i]);
			comp->SetInputCommand(GetInputCommand());
            oneBeatEmitter_[i] = comp.get();
            AddSceneComponent(std::move(comp));

            // パーティクルプールを初期化
            oneBeatEmitter_[i]->InitializeParticlePool(particlesPerBeat_);
        }
    }

    // 既存の bpmObject_ の初期化部分を置き換え
    {
        for (int i = 0; i < kBpmObjectCount; ++i) {
            auto modelData = ModelManager::GetModelDataFromFileName("bpmObject.obj");
            auto obj = std::make_unique<Model>(modelData);
            obj->SetName("bpmObject_" + std::to_string(i));

            if (auto *tr = obj->GetComponent3D<Transform3D>()) {
                // 各オブジェクトを少しずらして配置（必要に応じて調整）
                //float offset = static_cast<float>(i) * 0.5f;
                tr->SetTranslate(Vector3(10.0f, -10.0f, 10.0f));
                tr->SetScale(Vector3(1.0f));
            }

            if (auto *mt = obj->GetComponent3D<Material3D>()) {
				mt->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
                mt->SetEnableLighting(false);
            }

            if (screenBuffer3D)  obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (velocityBuffer)  obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");

            bpmObjects_[i] = obj.get();
            AddObject3D(std::move(obj));
        }

        bpmObjectStart_[0] = { -15.0f,0.0f,10.0f }; bpmObjectEnd_[0] = { -2.75f,0.0f,10.0f };
        bpmObjectStart_[1] = { 35.0f,0.0f,10.0f }; bpmObjectEnd_[1] = { 22.75f,0.0f,10.0f };
    }

    // Player（衝突判定を修正）
    {
        auto modelData = ModelManager::GetModelDataFromFileName("player.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Player");
        if (auto *tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(10.0f, 0.0f, 10.0f));
            tr->SetScale(Vector3(playerScaleMax_));
        }

        obj->RegisterComponent<PlayerMove>(2.0f, playerMoveDuration_);
        if (auto *playerArrowMove = obj->GetComponent3D<PlayerMove>()) {
            playerArrowMove->SetInputCommand(GetInputCommand());
            playerArrowMove->SetBPMToleranceRange(playerBpmToleranceRange_);
            playerArrowMove->SetMapSize(kMapW, kMapH);
        }

        obj->RegisterComponent<BPMScaling>(playerScaleMin_, playerScaleMax_, EaseType::EaseOutExpo);
        obj->RegisterComponent<Health>(3, 1.0f);

        // 衝突判定を追加（修正版）
        if (colliderComp && colliderComp->GetCollider()) {
            ColliderInfo3D info;
            Math::AABB aabb;
            aabb.min = Vector3{ -0.75f, -0.75f, -0.75f };
            aabb.max = Vector3{ +0.75f, +0.75f, +0.75f };
            info.shape = aabb;
            info.attribute.set(0);  // Player属性を設定

            obj->RegisterComponent<Collision3D>(colliderComp->GetCollider(), info);
        }

        if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        if (velocityBuffer) obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");
        player_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // ExplosionManager の追加
    {
        auto comp = std::make_unique<ExplosionManager>();
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetCollider(colliderComp);
        comp->SetCollider2(colliderComp);
        comp->SetExplosionLifetime(explosionLifetime_);
        comp->SetCameraController(cameraController_);  // カメラコントローラーを設定
        comp->SetCameraShake(bombShakePower_, bombShakeTime_);
        comp->SetSize(static_cast<float>(explosionSize_));
        explosionManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // ExplosionNumberDisplay の追加
    {
        auto comp = std::make_unique<ScoreDisplay>();
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetDisplayLifetime(explosionNumberDisplayLifetime_);
        comp->SetNumberScale(explosionNumberScale_);
        comp->SetYOffset(explosionNumberYOffset_);
        explosionNumberDisplay_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // ExplosionManagerにExplosionNumberDisplayを設定
    if (explosionManager_ && explosionNumberDisplay_) {
        explosionManager_->SetExplosionNumberDisplay(explosionNumberDisplay_);
    }

    // BombManager の追加
    {
        auto comp = std::make_unique<BombManager>(bombMaxNumber_);
        comp->SetPlayer(player_);
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetBPMToleranceRange(playerBpmToleranceRange_);
        comp->SetBombLifetimeBeats(bombLifetimeBeats_);
        comp->SetExplosionManager(explosionManager_);
        comp->SetNormalScaleRange(Vector3(bombNormalMinScale_), Vector3(bombNormalMaxScale_));
        comp->SetSpeedScaleRange(Vector3(bombSpeedMinScale_), Vector3(bombSpeedMaxScale_));
        comp->SetDetonationScale(Vector3(bombDetonationScale_));
        comp->SetMapSize(kMapW, kMapH);
        comp->SetInputCommand(GetInputCommand());
        comp->SetCollider(colliderComp);  // 追加
        bombManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    if (player_ && bombManager_) {
        if (auto *playerMove = player_->GetComponent3D<PlayerMove>()) {
            playerMove->SetBombManager(bombManager_);
        }
    }

    // PlayerのHealthコンポーネントにCameraControllerを設定
    if (player_ && cameraController_) {
        if (auto *health = player_->GetComponent3D<Health>()) {
            health->SetCameraController(cameraController_);
            health->SetShake(pDamageShakePower_, pDamageShakeTime_);
        }
    }

    // EnemyManagerの初期化（修正版）
    {
        auto comp = std::make_unique<EnemyManager>();
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetBPMSystem(bpmSystem_);
        comp->SetMapSize(kMapW, kMapH);
        comp->SetCollider(colliderComp);
        comp->SetPlayer(player_);
        comp->SetBombManager(bombManager_);
        enemyManager_ = comp.get();
        AddSceneComponent(std::move(comp));
        enemyManager_->InitializeParticlePool();
    }

    // ScoreManagerの初期化
    {
        auto comp = std::make_unique<ScoreManager>();
        scoreManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // ExplosionManagerにScoreManagerを設定
    if (explosionManager_ && scoreManager_) {
        explosionManager_->SetScoreManager(scoreManager_);
    }

    // EnemyManagerにScoreManagerのコールバックを設定（後方互換性のため保持）
    if (enemyManager_ && scoreManager_) {
        enemyManager_->SetOnEnemyDestroyedCallback([this]() {
            // このコールバックは現在使用されていないが、後方互換性のため保持
        });
    }

    // PlayerDieParticleManagerの初期化
    {
        auto comp = std::make_unique<PlayerDieParticleManager>();
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetPlayer(player_);
        comp->SetParticleConfig(playerDieParticleConfig_);
        playerDieParticleManager_ = comp.get();
        AddSceneComponent(std::move(comp));
        playerDieParticleManager_->InitializeParticlePool(playerDieParticleCount_);
    }

    {
        auto comp = std::make_unique<EnemySpawner>(enemySpawnInterval_);
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetEnemyManager(enemyManager_);
        comp->SetBPMSystem(bpmSystem_);
        enemySpawner_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    if (enemySpawner_) {
        constexpr float kTile = 2.0f;
        constexpr float kY = 0.0f;
        // パーティクルプールを初期化
        enemySpawner_->InitializeParticlePool(1); // 毎フレーム1個放出

        if (enemySpawner_) {
            //constexpr float kTile = 2.0f;
            //constexpr float kY = 0.0f;

            for (int x = 0; x < kMapW; ++x) {
                enemySpawner_->AddSpawnPoint(Vector3(kTile * x, kY, 0.0f));
                enemySpawner_->AddSpawnPoint(Vector3(kTile * x, kY, kTile * (kMapH - 1)));
            }

            for (int z = 1; z < kMapH - 1; ++z) {
                enemySpawner_->AddSpawnPoint(Vector3(0.0f, kY, kTile * z));
                enemySpawner_->AddSpawnPoint(Vector3(kTile * (kMapW - 1), kY, kTile * z));
            }
        }
    }

    // ExplosionManagerにBombManagerを設定（爆発とボムの衝突検出用）
    if (explosionManager_ && bombManager_) {
        explosionManager_->SetBombManager(bombManager_);
        explosionManager_->SetEnemyManager(enemyManager_);
    }

    // ExplosionManagerにPlayerを設定
    explosionManager_->SetPlayer(player_);

    // ステージのライティング用コンポーネント
    auto stageLighting = std::make_unique<StageLighting>();
    stageLighting_ = stageLighting.get();
    AddSceneComponent(std::move(stageLighting));

    // ステージ後ろの画面用
    AddSceneComponent(std::make_unique<BackMonitor>());

    // Debug: BackMonitor render components (game/menu/particle)
    {
        auto *bm = GetSceneComponent<BackMonitor>();
        if (bm) {
            auto compG = std::make_unique<BackMonitorWithGameScreen>(bm->GetScreenBuffer());
            backMonitorGame_ = compG.get();
            AddSceneComponent(std::move(compG));

            auto compM = std::make_unique<BackMonitorWithMenuScreen>(bm->GetScreenBuffer(), GetInputCommand());
            backMonitorMenu_ = compM.get();
            AddSceneComponent(std::move(compM));

            auto compP = std::make_unique<BackMonitorWithParticle>(bm->GetScreenBuffer());
            backMonitorParticle_ = compP.get();
            AddSceneComponent(std::move(compP));

            // set initial mode
            backMonitorMode_ = 0;
            if (backMonitorGame_) backMonitorGame_->SetActive(true);
            if (backMonitorMenu_) backMonitorMenu_->SetActive(false);
            if (backMonitorParticle_) backMonitorParticle_->SetActive(false);
        }
    }

    //==================================================
    // ↑ ここまでゲームオブジェクト定義
    //==================================================

    // Player Health UI (ライフ表示)
    {
        auto comp = std::make_unique<PlayerHealthUI>(screenBuffer2D);
        if (player_) {
            if (auto *health = player_->GetComponent3D<Health>()) {
                comp->SetHealth(health);
            }
        }
        AddSceneComponent(std::move(comp));
    }

    // Score UI (スコア表示)
    {
        auto comp = std::make_unique<ScoreUI>();
        if (scoreManager_) {
            comp->SetScoreManager(scoreManager_);
        }
        AddSceneComponent(std::move(comp));
    }

    // Player Health UI (ライフ表示)
    {
        auto comp = std::make_unique<PlayerHealthModelUI>(screenBuffer3D);
        if (player_) {
            if (auto *health = player_->GetComponent3D<Health>()) {
                comp->SetHealth(health);
            }
            if (auto *tr = camera3D->GetComponent3D<Transform3D>()) {
                comp->SetTransform(tr);
            }
            playerHealthUI_ = comp.get();
        }
        AddSceneComponent(std::move(comp));
    }

    // デバッグ用カメラ操作コンポーネント
    {
        auto comp = std::make_unique<DebugCameraMovement>(camera3D, GetInput());
        AddSceneComponent(std::move(comp));
    }
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
#if defined(USE_IMGUI)
    DrawObjectStateImGui();
    SetObjectValue();

    DrawParticleStateImGui();
    SetParticleValue();
#endif

	// プレイヤーの体力が0になった際の処理
    if (auto* health = player_->GetComponent3D<Health>()) {
        if (health->GetOutGameTimerIsFinished()) {

        }
    }

    if (auto *debugCameraMovement = GetSceneComponent<DebugCameraMovement>()) {
        if (GetInputCommand()->Evaluate("DebugCameraToggle").Triggered()) {
            debugCameraMovement->SetEnable(!debugCameraMovement->IsEnable());
        }
    }

    // Debug: toggle BackMonitor mode
    {
        auto r = GetInputCommand()->Evaluate("DebugChangeBackMonitor");
        if (r.Triggered()) {
            backMonitorMode_ = (backMonitorMode_ + 1) % 3;
            if (backMonitorGame_) backMonitorGame_->SetActive(backMonitorMode_ == 0);
            if (backMonitorMenu_) backMonitorMenu_->SetActive(backMonitorMode_ == 1);
            if (backMonitorParticle_) backMonitorParticle_->SetActive(backMonitorMode_ == 2);
        }
    }
    // Debug: StageLighting toggle
    if (stageLighting_) {
        if (GetInput()->GetKeyboard().IsTrigger(Key::L)) {
            if (stageLighting_->IsDeadLightingActive()) {
                stageLighting_->ResetLighting();
            } else {
                stageLighting_->StartDeadLighting();
            }
        }
    }

    // OnUpdate 内で BPM 進行度を更新
    if (bombManager_) {
        bombManager_->SetBPMProgress(bpmSystem_->GetBeatProgress());
    }

    if (player_) {
        if (auto *tr = player_->GetComponent3D<Transform3D>()) {
            playerMapX_ = static_cast<int>(tr->GetTranslate().x / 2.0f);
            playerMapZ_ = static_cast<int>(tr->GetTranslate().z / 2.0f);

            auto *playerArrowMove = player_->GetComponent3D<PlayerMove>();
            playerArrowMove->SetBPMProgress(bpmSystem_->GetBeatProgress());

            auto *bpmScaling = player_->GetComponent3D<BPMScaling>();
            if (bpmScaling) {
                bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
            }

            // BombSpawnコンポーネントにもBPM進行度を渡す
            if (auto *bombSpawn = player_->GetComponent3D<BombSpawn>()) {
                bombSpawn->SetBPMProgress(bpmSystem_->GetBeatProgress());
            }
        }
    }

    {
        if (allMapAnimation_) {
            // 全マップをアニメーション
            for (int z = 0; z < kMapH; z++) {
                for (int x = 0; x < kMapW; x++) {
                    if (maps_[z][x]) {
                        auto *bpmScaling = maps_[z][x]->GetComponent3D<BPMScaling>();
                        if (bpmScaling) {
                            bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
                        }
                    }
                }
            }
        } else {
            // 全マスを処理
            for (int z = 0; z < kMapH; z++) {
                for (int x = 0; x < kMapW; x++) {
                    if (maps_[z][x]) {
                        if (auto *tr = maps_[z][x]->GetComponent3D<Transform3D>()) {
                            // プレイヤーのいるマスだけアニメーション、それ以外は最大値に固定
                            auto *playerArrowMove = player_->GetComponent3D<PlayerMove>();
                            auto *bpmScaling = maps_[z][x]->GetComponent3D<BPMScaling>();
                            if (x == playerMapX_ && z == playerMapZ_ && !playerArrowMove->IsMoving()) {
                                bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
                            } else {
                                bpmScaling->SetBPMProgress(1.0f);
                            }
                        }
                    }
                }
            }
        }
    }

    {
        auto r = GetInputCommand()->Evaluate("DebugDestroyWindow");
        if (r.Triggered()) {
            if (auto *window = Window::GetWindow("2301_CLUBOM")) {
                window->DestroyNotify();
            }
        }
    }

    // デバッグ: 9キーでプレイヤー位置からDieParticleを発生
    {
        if (GetInput()->GetKeyboard().IsTrigger(Key::D9)) {
            if (player_ && playerDieParticleManager_) {
                if (auto* tr = player_->GetComponent3D<Transform3D>()) {
                    playerDieParticleManager_->SpawnParticles(tr->GetTranslate());
                }
            }
        }

        if (GetInput()->GetKeyboard().IsTrigger(Key::D8)) {
			InGameStart();
        }
    }

    // OnUpdate内の既存の bpmObject_ 更新部分を置き換え
    {
        if (bpmObjects_[0] && bpmObjects_[1]) {
            auto tr = bpmObjects_[0]->GetComponent3D<Transform3D>();
            auto tr1 = bpmObjects_[1]->GetComponent3D<Transform3D>();

            if (bpmSystem_->GetLeftRightToggle()) {
                tr->SetTranslate(Vector3(MyEasing::Lerp(bpmObjectStart_[0], bpmObjectEnd_[0], bpmSystem_->GetBeatProgress())));
                tr1->SetTranslate(bpmObjectStart_[1]);
            } else {
                tr1->SetTranslate(Vector3(MyEasing::Lerp(bpmObjectStart_[1], bpmObjectEnd_[1], bpmSystem_->GetBeatProgress())));
                tr->SetTranslate(bpmObjectStart_[0]);
            }
        }
    }

    {
        for (int i = 0; i < kOneBeatEmitterCount_; i++) {
			oneBeatEmitter_[i]->SetBPMBpmProgress(bpmSystem_->GetBeatProgress());
            oneBeatEmitter_[i]->SetBPMToleranceRange(playerBpmToleranceRange_);
        }

        if (bpmSystem_->GetLeftRightToggle()) {
            for (int i = 0; i < kOneBeatEmitterCount_ / 2; i++) {
                oneBeatEmitter_[i]->SetUseEmitter(true);
            }

            for (int i = 5; i < kOneBeatEmitterCount_; i++) {
                oneBeatEmitter_[i]->SetUseEmitter(false);
            }
        } else {
            for (int i = 0; i < kOneBeatEmitterCount_ / 2; i++) {
                oneBeatEmitter_[i]->SetUseEmitter(false);
            }

            for (int i = 5; i < kOneBeatEmitterCount_; i++) {
                oneBeatEmitter_[i]->SetUseEmitter(true);
            }
        }
    }

    if (nanidoTimer_.IsFinished()) {
        nanidoCount_--;
        if (nanidoCount_ <= 1) {
            nanidoCount_ = 1;
        }
    }
    nanidoTimer_.Update();

    if (enemySpawner_) {
        enemySpawner_->SetSpawnInterval(nanidoCount_);
    }
}

void GameScene::LoadObjectStateJson() {
    try {
        // JSONファイルから読み込み
        auto values = jsonManager_->Read(loadToSaveName_);

        if (values.empty()) {
            printf("[WARNING] Failed to load JSON file or file is empty: %s\n", loadToSaveName_.c_str());
            printf("[INFO] Using default emitter settings.\n");
            return; // 早期リターン
        }

        // 各フィールドを順番に取得して適用（try-catchで保護）
        size_t index = 0;

        // BPM関連
        if (index < values.size()) bpm_ = JsonManager::Reverse<int>(values[index++]);

        // マップ関連
        if (index < values.size()) mapScaleMin_.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) mapScaleMin_.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) mapScaleMin_.z = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) mapScaleMax_.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) mapScaleMax_.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) mapScaleMax_.z = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) allMapAnimation_ = JsonManager::Reverse<bool>(values[index++]);

        // カメラシェイク関連
        if (index < values.size()) pDamageShakePower_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) pDamageShakeTime_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombShakePower_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombShakeTime_ = JsonManager::Reverse<float>(values[index++]);

        // プレイヤー関連
        if (index < values.size()) playerScaleMin_.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerScaleMin_.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerScaleMin_.z = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerScaleMax_.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerScaleMax_.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerScaleMax_.z = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerBpmToleranceRange_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerMoveDuration_ = JsonManager::Reverse<float>(values[index++]);

        // 爆弾関連
        if (index < values.size()) bombMaxNumber_ = JsonManager::Reverse<int>(values[index++]);
        if (index < values.size()) bombLifetimeBeats_ = JsonManager::Reverse<int>(values[index++]);
        if (index < values.size()) bombNormalMinScale_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombNormalMaxScale_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombSpeedMinScale_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombSpeedMaxScale_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) bombDetonationScale_ = JsonManager::Reverse<float>(values[index++]);

        // 爆発関連
        if (index < values.size()) explosionLifetime_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) explosionSize_ = JsonManager::Reverse<int>(values[index++]);

        // ExplosionNumberDisplay関連
        if (index < values.size()) explosionNumberDisplayLifetime_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) explosionNumberScale_ = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) explosionNumberYOffset_ = JsonManager::Reverse<float>(values[index++]);

        // 敵関連
        if (index < values.size()) enemySpawnInterval_ = JsonManager::Reverse<int>(values[index++]);
    } catch (const std::exception &e) {
        printf("[ERROR] Critical error in LoadFromJson: %s\n", e.what());
        printf("[INFO] Using default emitter settings.\n");
        // デフォルト値はInitialize関数で既に設定済み
    }
}

void GameScene::SaveObjectStateJson() {
    // ディレクトリが存在しない場合は作成
    std::filesystem::path fullPath = jsonManager_->GetBasePath() + loadToSaveName_ + ".json";
    std::filesystem::path directory = fullPath.parent_path();

    if (!directory.empty() && !std::filesystem::exists(directory)) {
        try {
            std::filesystem::create_directories(directory);
            printf("[INFO] Created directory: %s\n", directory.string().c_str());
        } catch (const std::filesystem::filesystem_error &e) {
            printf("[ERROR] Failed to create directory: %s\n", e.what());
            return;
        }
    }

    // BPM関連
    jsonManager_->RegistOutput(bpm_, "bpm");

    // マップ関連
    jsonManager_->RegistOutput(mapScaleMin_.x, "mapScaleMin_x");
    jsonManager_->RegistOutput(mapScaleMin_.y, "mapScaleMin_y");
    jsonManager_->RegistOutput(mapScaleMin_.z, "mapScaleMin_z");
    jsonManager_->RegistOutput(mapScaleMax_.x, "mapScaleMax_x");
    jsonManager_->RegistOutput(mapScaleMax_.y, "mapScaleMax_y");
    jsonManager_->RegistOutput(mapScaleMax_.z, "mapScaleMax_z");
    jsonManager_->RegistOutput(allMapAnimation_, "allMapAnimation");

    // カメラシェイク関連
    jsonManager_->RegistOutput(pDamageShakePower_, "pDamageShakePower");
    jsonManager_->RegistOutput(pDamageShakeTime_, "pDamageShakeTime");
    jsonManager_->RegistOutput(bombShakePower_, "bombShakePower");
    jsonManager_->RegistOutput(bombShakeTime_, "bombShakeTime");

    // プレイヤー関連
    jsonManager_->RegistOutput(playerScaleMin_.x, "playerScaleMin_x");
    jsonManager_->RegistOutput(playerScaleMin_.y, "playerScaleMin_y");
    jsonManager_->RegistOutput(playerScaleMin_.z, "playerScaleMin_z");
    jsonManager_->RegistOutput(playerScaleMax_.x, "playerScaleMax_x");
    jsonManager_->RegistOutput(playerScaleMax_.y, "playerScaleMax_y");
    jsonManager_->RegistOutput(playerScaleMax_.z, "playerScaleMax_z");
    jsonManager_->RegistOutput(playerBpmToleranceRange_, "playerBpmToleranceRange");
    jsonManager_->RegistOutput(playerMoveDuration_, "playerMoveDuration");

    // 爆弾関連
    jsonManager_->RegistOutput(bombMaxNumber_, "bombMaxNumber");
    jsonManager_->RegistOutput(bombLifetimeBeats_, "bombLifetimeBeats");
    jsonManager_->RegistOutput(bombNormalMinScale_, "bombNormalMinScale");
    jsonManager_->RegistOutput(bombNormalMaxScale_, "bombNormalMaxScale");
    jsonManager_->RegistOutput(bombSpeedMinScale_, "bombSpeedMinScale");
    jsonManager_->RegistOutput(bombSpeedMaxScale_, "bombSpeedMaxScale");
    jsonManager_->RegistOutput(bombDetonationScale_, "bombDetonationScale");

    // 爆発関連
    jsonManager_->RegistOutput(explosionLifetime_, "explosionLifetime");
    jsonManager_->RegistOutput(explosionSize_, "explosionSize");

    // ExplosionNumberDisplay関連
    jsonManager_->RegistOutput(explosionNumberDisplayLifetime_, "explosionNumberDisplayLifetime");
    jsonManager_->RegistOutput(explosionNumberScale_, "explosionNumberScale");
    jsonManager_->RegistOutput(explosionNumberYOffset_, "explosionNumberYOffset");

    // 敵関連
    jsonManager_->RegistOutput(enemySpawnInterval_, "enemySpawnInterval");

    // JSONファイルに書き込み
    jsonManager_->Write(loadToSaveName_);
}

void GameScene::LoadParticleStateJson() {
    try {
        // JSONファイルから読み込み（jsonParticleManager_を使用）
        auto values = jsonParticleManager_->Read(loadToSaveParticleName_);

        if (values.empty()) {
            printf("[WARNING] Failed to load ParticleState JSON file or file is empty\n");
            printf("[INFO] Using default particle settings.\n");
            return; // 早期リターン
        }

        size_t index = 0;

        // EnemySpawnParticle関連
        if (index < values.size()) enemySpawnParticleConfig_.initialSpeed = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.speedVariation = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.lifeTimeSec = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.gravity = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.damping = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.baseScale.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.baseScale.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemySpawnParticleConfig_.baseScale.z = JsonManager::Reverse<float>(values[index++]);

        // EnemyDieParticle関連
        if (index < values.size()) enemyDieParticleConfig_.initialSpeed = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.speedVariation = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.lifeTimeSec = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.gravity = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.damping = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.baseScale.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.baseScale.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) enemyDieParticleConfig_.baseScale.z = JsonManager::Reverse<float>(values[index++]);

        // OneBeatParticle関連
        if (index < values.size()) oneBeatParticleConfig_.initialSpeed = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.speedVariation = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.lifeTimeSec = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.gravity = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.damping = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.spreadAngle = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.baseScale.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.baseScale.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatParticleConfig_.baseScale.z = JsonManager::Reverse<float>(values[index++]);

        // OneBeatMissParticle関連
        if (index < values.size()) oneBeatMissParticleConfig_.initialSpeed = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.speedVariation = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.lifeTimeSec = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.gravity = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.damping = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.spreadAngle = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.baseScale.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.baseScale.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) oneBeatMissParticleConfig_.baseScale.z = JsonManager::Reverse<float>(values[index++]);

        // PlayerDieParticle関連
        if (index < values.size()) playerDieParticleConfig_.initialSpeed = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.speedVariation = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.lifeTimeSec = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.gravity = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.damping = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.spreadAngle = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.baseScale.x = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.baseScale.y = JsonManager::Reverse<float>(values[index++]);
        if (index < values.size()) playerDieParticleConfig_.baseScale.z = JsonManager::Reverse<float>(values[index++]);
    } catch (const std::exception &e) {
        printf("[ERROR] Critical error in LoadParticleStateJson: %s\n", e.what());
        printf("[INFO] Using default particle settings.\n");
    }
}

void GameScene::SaveParticleStateJson() {
    // ディレクトリが存在しない場合は作成（jsonParticleManager_を使用）
    std::filesystem::path fullPath = jsonParticleManager_->GetBasePath() + loadToSaveParticleName_ + ".json";
    std::filesystem::path directory = fullPath.parent_path();

    if (!directory.empty() && !std::filesystem::exists(directory)) {
        try {
            std::filesystem::create_directories(directory);
            printf("[INFO] Created directory: %s\n", directory.string().c_str());
        } catch (const std::filesystem::filesystem_error &e) {
            printf("[ERROR] Failed to create directory: %s\n", e.what());
            return;
        }
    }

    // EnemySpawnParticle関連
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.initialSpeed, "enemySpawn_initialSpeed");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.speedVariation, "enemySpawn_speedVariation");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.lifeTimeSec, "enemySpawn_lifeTimeSec");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.gravity, "enemySpawn_gravity");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.damping, "enemySpawn_damping");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.baseScale.x, "enemySpawn_baseScale_x");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.baseScale.y, "enemySpawn_baseScale_y");
    jsonParticleManager_->RegistOutput(enemySpawnParticleConfig_.baseScale.z, "enemySpawn_baseScale_z");

    // EnemyDieParticle関連
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.initialSpeed, "enemyDie_initialSpeed");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.speedVariation, "enemyDie_speedVariation");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.lifeTimeSec, "enemyDie_lifeTimeSec");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.gravity, "enemyDie_gravity");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.damping, "enemyDie_damping");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.baseScale.x, "enemyDie_baseScale_x");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.baseScale.y, "enemyDie_baseScale_y");
    jsonParticleManager_->RegistOutput(enemyDieParticleConfig_.baseScale.z, "enemyDie_baseScale_z");

    // OneBeatParticle関連
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.initialSpeed, "oneBeat_initialSpeed");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.speedVariation, "oneBeat_speedVariation");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.lifeTimeSec, "oneBeat_lifeTimeSec");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.gravity, "oneBeat_gravity");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.damping, "oneBeat_damping");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.spreadAngle, "oneBeat_spreadAngle");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.baseScale.x, "oneBeat_baseScale_x");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.baseScale.y, "oneBeat_baseScale_y");
    jsonParticleManager_->RegistOutput(oneBeatParticleConfig_.baseScale.z, "oneBeat_baseScale_z");

    // OneBeatMissParticle関連
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.initialSpeed, "oneBeatMiss_initialSpeed");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.speedVariation, "oneBeatMiss_speedVariation");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.lifeTimeSec, "oneBeatMiss_lifeTimeSec");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.gravity, "oneBeatMiss_gravity");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.damping, "oneBeatMiss_damping");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.spreadAngle, "oneBeatMiss_spreadAngle");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.baseScale.x, "oneBeatMiss_baseScale_x");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.baseScale.y, "oneBeatMiss_baseScale_y");
    jsonParticleManager_->RegistOutput(oneBeatMissParticleConfig_.baseScale.z, "oneBeatMiss_baseScale_z");

    // PlayerDieParticle関連
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.initialSpeed, "playerDie_initialSpeed");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.speedVariation, "playerDie_speedVariation");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.lifeTimeSec, "playerDie_lifeTimeSec");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.gravity, "playerDie_gravity");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.damping, "playerDie_damping");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.spreadAngle, "playerDie_spreadAngle");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.baseScale.x, "playerDie_baseScale_x");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.baseScale.y, "playerDie_baseScale_y");
    jsonParticleManager_->RegistOutput(playerDieParticleConfig_.baseScale.z, "playerDie_baseScale_z");

    // JSONファイルに書き込み（jsonParticleManager_を使用）
    jsonParticleManager_->Write(loadToSaveParticleName_);
}

#if defined(USE_IMGUI)
void GameScene::SetObjectValue() {
    // BPMシステムの更新
    if (bpmSystem_) {
        bpmSystem_->SetBPM(static_cast<float>(bpm_));
    }

    // マップのスケール更新
    for (int z = 0; z < kMapH; z++) {
        for (int x = 0; x < kMapW; x++) {
            if (maps_[z][x]) {
                if (auto *bpmScaling = maps_[z][x]->GetComponent3D<BPMScaling>()) {
                    bpmScaling->SetMinMaxScale(mapScaleMin_, mapScaleMax_);
                }
            }
        }
    }

    // プレイヤーのスケール更新
    if (player_) {
        if (auto *bpmScaling = player_->GetComponent3D<BPMScaling>()) {
            bpmScaling->SetMinMaxScale(playerScaleMin_, playerScaleMax_);
        }

        // プレイヤーの移動設定更新
        if (auto *playerMove = player_->GetComponent3D<PlayerMove>()) {
            playerMove->SetBPMToleranceRange(playerBpmToleranceRange_);
            playerMove->SetMoveDuration(playerMoveDuration_);
        }

        // プレイヤーのダメージ時カメラシェイク更新
        if (auto *health = player_->GetComponent3D<Health>()) {
            health->SetShake(pDamageShakePower_, pDamageShakeTime_);
        }
    }

    // ボム関連の設定更新
    if (bombManager_) {
        bombManager_->SetMaxBombs(bombMaxNumber_);
        bombManager_->SetBombLifetimeBeats(bombLifetimeBeats_);
        bombManager_->SetBPMToleranceRange(playerBpmToleranceRange_);
        bombManager_->SetBeatDuration(bpmSystem_->GetBeatDuration());
        bombManager_->SetNormalScaleRange(Vector3(bombNormalMinScale_), Vector3(bombNormalMaxScale_));
        bombManager_->SetSpeedScaleRange(Vector3(bombSpeedMinScale_), Vector3(bombSpeedMaxScale_));
        bombManager_->SetDetonationScale(Vector3(bombDetonationScale_));
    }

    // 爆発関連の設定更新
    if (explosionManager_) {
        explosionManager_->SetExplosionLifetime(explosionLifetime_);
        explosionManager_->SetCameraShake(bombShakePower_, bombShakeTime_);
        explosionManager_->SetSize(static_cast<float>(explosionSize_));
    }

    // ExplosionNumberDisplay関連の設定更新
    if (explosionNumberDisplay_) {
        explosionNumberDisplay_->SetDisplayLifetime(explosionNumberDisplayLifetime_);
        explosionNumberDisplay_->SetNumberScale(explosionNumberScale_);
        explosionNumberDisplay_->SetYOffset(explosionNumberYOffset_);
    }

    // 敵スポーン間隔の更新
    if (enemySpawner_) {
        enemySpawner_->SetSpawnInterval(enemySpawnInterval_);
    }

    // SetValue() 内
    if (bpmObjects_[0]) {  // 代表として最初のオブジェクトをチェック
        for (int i = 0; i < kBpmObjectCount; ++i) {
            if (bpmObjects_[i]) {
                if (auto *bpmScaling = bpmObjects_[i]->GetComponent3D<BPMScaling>()) {
                    bpmScaling->SetMinMaxScale(bpmObjectStart_[i], bpmObjectEnd_[i]);
                }
            }
        }
    }
}

void GameScene::DrawObjectStateImGui() {
    ImGui::Begin("ObjectStatus");

    if (ImGui::Button("Jsonに値を保存")) {
        SaveObjectStateJson();
    }

    ImGui::SameLine();
    ImGui::Separator();

    // BPM関連
    if (ImGui::CollapsingHeader("BPM関連", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragInt("BPM値", &bpm_, 1.0f, 60, 240);
        ImGui::DragFloat3("BpmObjMin0", &bpmObjectStart_[0].x, 0.01f);
        ImGui::DragFloat3("BpmObjMax0", &bpmObjectEnd_[0].x, 0.01f);
        ImGui::DragFloat3("BpmObjMin1", &bpmObjectStart_[1].x, 0.01f);
        ImGui::DragFloat3("BpmObjMax1", &bpmObjectEnd_[1].x, 0.01f);
        ImGui::DragFloat3("BpmObjMin2", &bpmObjectStart_[2].x, 0.01f);
        ImGui::DragFloat3("BpmObjMax2", &bpmObjectEnd_[2].x, 0.01f);
        ImGui::DragFloat3("BpmObjMin3", &bpmObjectStart_[3].x, 0.01f);
        ImGui::DragFloat3("BpmObjMax3", &bpmObjectEnd_[3].x, 0.01f);
    }

    // マップ関連
    if (ImGui::CollapsingHeader("マップ関連")) {
        ImGui::DragFloat3("マップ最小スケール", &mapScaleMin_.x, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("マップ最大スケール", &mapScaleMax_.x, 0.01f, 0.0f, 10.0f);
        ImGui::Checkbox("全マップアニメーション", &allMapAnimation_);
    }

    // カメラシェイク関連
    if (ImGui::CollapsingHeader("カメラシェイク")) {
        ImGui::DragFloat("プレイヤーダメージ:シェイクパワー", &pDamageShakePower_, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("プレイヤーダメージ:シェイク時間", &pDamageShakeTime_, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("爆弾爆発:シェイクパワー", &bombShakePower_, 0.01f, 0.0f, 100.0f);
        ImGui::DragFloat("爆弾爆発:シェイク時間", &bombShakeTime_, 0.01f, 0.0f, 100.0f);
    }

    // プレイヤー関連
    if (ImGui::CollapsingHeader("プレイヤー関連")) {
        ImGui::DragFloat3("プレイヤー最小スケール", &playerScaleMin_.x, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("プレイヤー最大スケール", &playerScaleMax_.x, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("BPM許容範囲", &playerBpmToleranceRange_, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("移動時間", &playerMoveDuration_, 0.01f, 0.0f, 1.0f);
        ImGui::Text("マップ座標: X=%d, Z=%d", playerMapX_, playerMapZ_);
    }

    // 爆弾関連
    if (ImGui::CollapsingHeader("爆弾関連")) {
        ImGui::DragInt("最大設置数", &bombMaxNumber_, 1.0f, 1, 10);
        ImGui::DragInt("寿命(拍数)", &bombLifetimeBeats_, 1.0f, 1, 100);
        ImGui::DragFloat("通常時拡小スケール", &bombNormalMinScale_, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("通常時拡大スケール", &bombNormalMaxScale_, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("起爆前拡小スケール", &bombSpeedMinScale_, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("起爆前拡大スケール", &bombSpeedMaxScale_, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("起爆時スケール", &bombDetonationScale_, 0.01f, 0.0f, 10.0f);
    }

    // 爆発関連
    if (ImGui::CollapsingHeader("爆発関連")) {
        ImGui::DragFloat("爆発寿命(秒)", &explosionLifetime_, 0.01f, 0.1f, 5.0f);
        ImGui::DragInt("爆発サイズ(XZ)", &explosionSize_, 1.0f, 1, 10);
    }

    // ExplosionNumberDisplay関連
    if (ImGui::CollapsingHeader("ScoreModel関連")) {
        ImGui::DragFloat("表示寿命(秒)", &explosionNumberDisplayLifetime_, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("スコール", &explosionNumberScale_, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("Y軸オフセット", &explosionNumberYOffset_, 0.01f, -10.0f, 10.0f);
    }

    // 敵関連
    if (ImGui::CollapsingHeader("敵関連")) {
        ImGui::DragInt("スポーン間隔(拍数)", &enemySpawnInterval_, 1.0f, 1, 100);
    }

    ImGui::End();
}

void GameScene::SetParticleValue() {

    for (int i = 0; i < kOneBeatEmitterCount_; ++i) {
        if (oneBeatEmitter_[i]) {
            oneBeatEmitter_[i]->SetParticleConfig(oneBeatParticleConfig_);
			oneBeatEmitter_[i]->SetMissParticleConfig(oneBeatMissParticleConfig_);
        }
    }

    if (enemySpawner_) {
        enemySpawner_->SetEnemyDieParticleConfig(enemySpawnParticleConfig_);
    }

    if (enemyManager_) {
        enemyManager_->SetDieParticleConfig(enemyDieParticleConfig_);
    }

    if (playerDieParticleManager_) {
        playerDieParticleManager_->SetParticleConfig(playerDieParticleConfig_);
    }
}

void GameScene::DrawParticleStateImGui() {
    ImGui::Begin("ParticleStatus");

    if (ImGui::Button("Jsonに値を保存")) {
        SaveParticleStateJson();
    }

    ImGui::SameLine();
    ImGui::Separator();

    // EnemySpawnParticle関連
    if (ImGui::CollapsingHeader("EnemySpawnParticle", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("初速度##spawn", &enemySpawnParticleConfig_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("速度ランダム幅##spawn", &enemySpawnParticleConfig_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("生存時間(秒)##spawn", &enemySpawnParticleConfig_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("重力##spawn", &enemySpawnParticleConfig_.gravity, 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("減衰率##spawn", &enemySpawnParticleConfig_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat3("基本スケール##spawn", &enemySpawnParticleConfig_.baseScale.x, 0.01f, 0.0f, 5.0f);
    }

    // EnemyDieParticle関連
    if (ImGui::CollapsingHeader("EnemyDieParticle")) {
        ImGui::DragFloat("初速度##die", &enemyDieParticleConfig_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("速度ランダム幅##die", &enemyDieParticleConfig_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("生存時間(秒)##die", &enemyDieParticleConfig_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("重力##die", &enemyDieParticleConfig_.gravity, 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("減衰率##die", &enemyDieParticleConfig_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat3("基本スケール##die", &enemyDieParticleConfig_.baseScale.x, 0.01f, 0.0f, 5.0f);
    }

    // OneBeatParticle関連
    if (ImGui::CollapsingHeader("OneBeatParticle")) {
        ImGui::DragFloat("初速度##onebeat", &oneBeatParticleConfig_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("速度ランダム幅##onebeat", &oneBeatParticleConfig_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("生存時間(秒)##onebeat", &oneBeatParticleConfig_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("重力##onebeat", &oneBeatParticleConfig_.gravity, 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("減衰率##onebeat", &oneBeatParticleConfig_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("広がる角度(度)##onebeat", &oneBeatParticleConfig_.spreadAngle, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat3("基本スケール##onebeat", &oneBeatParticleConfig_.baseScale.x, 0.01f, 0.0f, 5.0f);
    }

    // OneBeatMissParticle関連
    if (ImGui::CollapsingHeader("OneBeatMissParticle")) {
        ImGui::DragFloat("初速度##onebeatmiss", &oneBeatMissParticleConfig_.initialSpeed, 0.1f, 0.0f, 20.0f);
        ImGui::DragFloat("速度ランダム幅##onebeatmiss", &oneBeatMissParticleConfig_.speedVariation, 0.1f, 0.0f, 10.0f);
        ImGui::DragFloat("生存時間(秒)##onebeatmiss", &oneBeatMissParticleConfig_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
        ImGui::DragFloat("重力##onebeatmiss", &oneBeatMissParticleConfig_.gravity, 0.1f, -20.0f, 20.0f);
        ImGui::DragFloat("減衰率##onebeatmiss", &oneBeatMissParticleConfig_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("広がる角度(度)##onebeatmiss", &oneBeatMissParticleConfig_.spreadAngle, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat3("基本スケール##onebeatmiss", &oneBeatMissParticleConfig_.baseScale.x, 0.01f, 0.0f, 5.0f);
    }

    // PlayerDieParticle関連
    if (ImGui::CollapsingHeader("PlayerDieParticle")) {
        ImGui::DragFloat("初速度##playerdie", &playerDieParticleConfig_.initialSpeed, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("速度ランダム幅##playerdie", &playerDieParticleConfig_.speedVariation, 0.1f, 0.0f, 100.0f);
        ImGui::DragFloat("生存時間(秒)##playerdie", &playerDieParticleConfig_.lifeTimeSec, 0.01f, 0.1f, 10.0f);
        ImGui::DragFloat("重力##playerdie", &playerDieParticleConfig_.gravity, 0.1f, -100.0f, 100.0f);
        ImGui::DragFloat("減衰率##playerdie", &playerDieParticleConfig_.damping, 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat("広がる角度(度)##playerdie", &playerDieParticleConfig_.spreadAngle, 1.0f, 0.0f, 180.0f);
        ImGui::DragFloat3("基本スケール##playerdie", &playerDieParticleConfig_.baseScale.x, 0.01f, 0.0f, 10.0f);
    }

    ImGui::End();
}
#endif

void GameScene::InGameStart() {
    if (!isGameStarted_) {
        isGameStarted_ = true;
        auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Audio/GameBGM_120BPM.mp3");
        if (handle == AudioManager::kInvalidSoundHandle) {
            // 音声が未ロードならログ出力するか無视（ここでは無害に戻す）
            return;
        }
        AudioManager::Play(handle, 0.2f, 0.0f, true);

        if (bpmSystem_) {
            bpmSystem_->MeasurementStart();
        }

        if (auto* move = player_->GetComponent3D<PlayerMove>()) {
            move->SetisStarted(true);
        }

        if (bombManager_) {
            bombManager_->SetIsStarted(true);
        }

        if (enemySpawner_) {
            enemySpawner_->SetIsStarted(true);
        }

        nanidoTimer_.Start(20.0f, true);
    }
}

}