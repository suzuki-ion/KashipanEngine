#include "Scenes/TestScene.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Objects/Components/Player/BpmbSpawn.h"
#include "objects/Components/Health.h"
#include "Objects/SystemObjects/LightManager.h"
#include <cmath>

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
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
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer3D->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 0.0f;
        bp.softKnee = 0.0f;
        bp.intensity = 0.0f;
        bp.blurRadius = 0.0f;
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
    directionalLight->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    directionalLight->SetDirection(Vector3(1.8f, -2.0f, 1.2f));
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

    // map (Box scaled)
    {
        for (int z = 0; z < kMapH; z++) {
            for (int x = 0; x < kMapW; x++) {

                auto modelData = ModelManager::GetModelDataFromFileName("block.obj");
                auto obj = std::make_unique<Model>(modelData);

                obj->SetName("Map" ":x" + std::to_string(x) + ":z" + std::to_string(z));

                obj->RegisterComponent<BPMScaling>(mapScaleMin_, mapScaleMax_,EaseType::EaseOutExpo);

                if (auto* tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(Vector3(2.0f * x, 0.0f, 2.0f * z));
                    tr->SetScale(Vector3(mapScaleMax_));
                }

                //if (auto* mat = obj->GetComponent3D<Material3D>()) {
                //    mat->SetTexture(TextureManager::GetTextureFromFileName("uvChecker.png"));
                //}

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
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(10.0f, -1.0f, 10.0f));
			tr->SetScale(Vector3(1.0f));
        }

        if (screenBuffer3D)  obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
        //if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
        if (velocityBuffer)  obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");
        stage_ = obj.get();
        AddObject3D(std::move(obj));
    }

    // Player（衝突判定を修正）
    {
        auto modelData = ModelManager::GetModelDataFromFileName("Player.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Player");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 0.0f, 0.0f));
            tr->SetScale(Vector3(playerScaleMax_));
        }

        obj->RegisterComponent<PlayerMove>(2.0f, playerMoveDuration_);
        if (auto* playerArrowMove = obj->GetComponent3D<PlayerMove>()) {
            playerArrowMove->SetInputCommand(GetInputCommand());
            playerArrowMove->SetBPMToleranceRange(playerBpmToleranceRange_);
            playerArrowMove->SetMapSize(kMapW, kMapH);
        }

        obj->RegisterComponent<BPMScaling>(playerScaleMin_, playerScaleMax_, EaseType::EaseOutExpo);
        obj->RegisterComponent<Health>(10, 1.0f);

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
        explosionManager_ = comp.get();
        AddSceneComponent(std::move(comp));
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
        comp->SetMapSize(kMapW, kMapH);
        comp->SetInputCommand(GetInputCommand());
        comp->SetCollider(colliderComp);  // 追加
        bombManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    if (player_ && bombManager_) {
        if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
            playerMove->SetBombManager(bombManager_);
        }
    }

    // PlayerのHealthコンポーネントにCameraControllerを設定
    if (player_ && cameraController_) {
        if (auto* health = player_->GetComponent3D<Health>()) {
            health->SetCameraController(cameraController_);
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

    {
        auto comp = std::make_unique<EnemySpawner>(enemySpawnInterval_);
        comp->SetScreenBuffer(screenBuffer3D);
        comp->SetShadowMapBuffer(shadowMapBuffer);
        comp->SetEnemyManager(enemyManager_);
        comp->SetBPMSystem(bpmSystem_);
        enemySpawner_ = comp.get();
        AddSceneComponent(std::move(comp));

        if (enemySpawner_) {
            constexpr float kTile = 2.0f;
            constexpr float kY = 0.0f;

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

    //if (playBgm_) {
    //    auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Sounds/BPM120.wav");
    //    if (handle == AudioManager::kInvalidSoundHandle) {
    //        // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
    //        return;
    //    }
    //    AudioManager::Play(handle, 0.05f, 0.0f, true);
    //}

    // Particles
    /*{
        ParticleMovement::SpawnBox spawn;
        spawn.min = Vector3(-16.0f, 0.0f, -16.0f);
        spawn.max = Vector3(16.0f, 16.0f, 16.0f);

        int instanceCount = 128;

        particleLights_.clear();
        particleLights_.reserve(instanceCount);

        for (int i = 0; i < instanceCount; ++i) {
            auto obj = std::make_unique<Box>();
            obj->SetName("ParticleBox_" + std::to_string(i));
            obj->RegisterComponent<ParticleMovement>(spawn, 0.5f, 10.0f, Vector3{0.5f, 0.5f, 0.5f});

            if (auto* mat = obj->GetComponent3D<Material3D>()) {
                mat->SetEnableLighting(true);
                mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
            }

            if (screenBuffer3D) obj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer) obj->AttachToRenderer(shadowMapBuffer, "Object3D.ShadowMap.DepthOnly");
            if (velocityBuffer) obj->AttachToRenderer(velocityBuffer, "Object3D.Velocity");

            auto* particlePtr = obj.get();
            AddObject3D(std::move(obj));

            auto lightObj = std::make_unique<PointLight>();
            lightObj->SetName("ParticlePointLight_" + std::to_string(i));
            lightObj->SetEnabled(true);
            lightObj->SetColor(particleLightColor_);
            lightObj->SetIntensity(0.0f);
            lightObj->SetRange(0.0f);

            auto* lightPtr = lightObj.get();
            if (screenBuffer3D) lightObj->AttachToRenderer(screenBuffer3D, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(lightObj));

            particleLights_.push_back(ParticleLightPair{ particlePtr, lightPtr });
            lightManager->AddPointLight(lightPtr);
        }
    }*/

    //==================================================
    // ↑ ここまでゲームオブジェクト定義 ↑
    //==================================================

    // Player Health UI (ライフ表示)
    {
        auto comp = std::make_unique<PlayerHealthUI>(screenBuffer2D);
        if (player_) {
            if (auto* health = player_->GetComponent3D<Health>()) {
                comp->SetHealth(health);
            }
            playerHealthUI_ = comp.get();
        }
        AddSceneComponent(std::move(comp));
    }
}

TestScene::~TestScene() {
}

void TestScene::OnUpdate() {
#if defined(USE_IMGUI)
    DrawImGui();
#endif

    // OnUpdate 内で BPM 進行度を更新
    if (bombManager_) {
        bombManager_->SetBPMProgress(bpmSystem_->GetBeatProgress());
    }

    if (player_) {
        if (auto* tr = player_->GetComponent3D<Transform3D>()) {
			playerMapX_ = static_cast<int>(tr->GetTranslate().x / 2.0f);
			playerMapZ_ = static_cast<int>(tr->GetTranslate().z / 2.0f);

            auto* playerArrowMove = player_->GetComponent3D<PlayerMove>();
			playerArrowMove->SetBPMProgress(bpmSystem_->GetBeatProgress());
            
            if (auto* mt = player_->GetComponent3D<Material3D>()) {
                if (auto* health = player_->GetComponent3D<Health>()) {

                    // 残り時間(秒)を 0.1 秒単位の整数にする（誤差対策で丸め）
                    int tick = static_cast<int>(health->GetDamageCooldownRemaining() * 10.0f + 0.5f);

                    if (health->WasDamagedThisCooldown() && (tick % 2 == 0)) {
                        mt->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f }); // 透明
                    } else {
                        mt->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // 不透明
                    }
                }
            }


			auto* bpmScaling = player_->GetComponent3D<BPMScaling>();
			if (bpmScaling) {
				bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
			}

            // BombSpawnコンポーネントにもBPM進行度を渡す
            if (auto* bombSpawn = player_->GetComponent3D<BombSpawn>()) {
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
						auto* bpmScaling = maps_[z][x]->GetComponent3D<BPMScaling>();
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
                        if (auto* tr = maps_[z][x]->GetComponent3D<Transform3D>()) {
                            // プレイヤーのいるマスだけアニメーション、それ以外は最大値に固定
                            auto* playerArrowMove = player_->GetComponent3D<PlayerMove>();
                            auto* bpmScaling = maps_[z][x]->GetComponent3D<BPMScaling>();
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

    // Particle -> PointLight sync (position + lifetime-driven params)
    {
        for (auto &pl : particleLights_) {
            if (!pl.particle || !pl.light) continue;

            auto* tr = pl.particle->GetComponent3D<Transform3D>();
            auto* pm = pl.particle->GetComponent3D<ParticleMovement>();
            if (!tr || !pm) continue;

            const Vector3 pos = tr->GetTranslate();
            pl.light->SetPosition(pos);

            const float t = pm->GetNormalizedLife(); // 0..1
            float life01 = 0.0f;
            if (t < 0.5f) {
                life01 = (t / 0.5f);
            } else {
                life01 = (1.0f - (t - 0.5f) / 0.5f);
            }
            life01 = std::clamp(life01, 0.0f, 1.0f);

            const float eased = EaseOutCubic(0.0f, 1.0f, life01);
            pl.light->SetIntensity(particleLightIntensityMin_ + (particleLightIntensityMax_ - particleLightIntensityMin_) * eased);
            pl.light->SetRange(particleLightRangeMin_ + (particleLightRangeMax_ - particleLightRangeMin_) * eased);
        }
    }

    {
        auto r = GetInputCommand()->Evaluate("DebugDestroyWindow");
        if (r.Triggered()) {
            if (auto *window = Window::GetWindow("Main Window")) {
                window->DestroyNotify();
            }
        }
    }
}

#if defined(USE_IMGUI)
void TestScene::DrawImGui() {
    ImGui::Begin("ObjectStatus");

    ImGui::DragInt("BPM値", &bpm_);

    ImGui::End();
}
#endif

} // namespace KashipanEngine