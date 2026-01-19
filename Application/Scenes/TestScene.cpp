#include "Scenes/TestScene.h"
#include "Scenes/Components/ScreenBufferKeepRatio.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "Objects/Components/ParticleMovement.h"
#include "Objects/SystemObjects/ShadowMapBinder.h"
#include "Scene/Components/ShadowMapCameraSync.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Objects/Components/Player/BpmbSpawn.h"
#include "objects/Components/Health.h"
#include "Objects/SystemObjects/LightManager.h"

namespace KashipanEngine {

TestScene::TestScene()
    : SceneBase("TestScene") {
}

void TestScene::Initialize() {
    velocityBuffer_ = ScreenBuffer::Create(1920, 1080);
    screenBuffer_ = ScreenBuffer::Create(1920, 1080);
    shadowMapBuffer_ = ShadowMapBuffer::Create(2048, 2048);

    if (screenBuffer_) {
        ChromaticAberrationEffect::Params p{};
        p.directionX = 1.0f;
        p.directionY = 0.0f;
        p.strength = 0.001f;
        screenBuffer_->RegisterPostEffectComponent(std::make_unique<ChromaticAberrationEffect>(p));

        BloomEffect::Params bp{};
        bp.threshold = 0.0f;
        bp.softKnee = 0.0f;
        bp.intensity = 0.0f;
        bp.blurRadius = 0.0f;
        bp.iterations = 4;
        screenBuffer_->RegisterPostEffectComponent(std::make_unique<BloomEffect>(bp));
        
        screenBuffer_->AttachToRenderer("ScreenBuffer_TitleScene");
    }
    auto* window = Window::GetWindow("Main Window");

    // ColliderComponentを追加（一番最初に追加）
    {
        auto comp = std::make_unique<ColliderComponent>();
        collider_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

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
        obj->SetColor(Vector4(0.6f, 0.5f, 1.0f, 1.0f));
        obj->SetDirection(Vector3(1.8f, -2.0f, 1.2f));
        obj->SetIntensity(0.3f);
        light_ = obj.get();

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        AddObject3D(std::move(obj));
    }

    // LightManager (screenBuffer_)
    LightManager* lightManager = nullptr;
    {
        auto obj = std::make_unique<LightManager>();
        obj->SetName("LightManager");
        lightManager = obj.get();
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

    // Velocity buffer camera binder
    {
        auto vbBinder = std::make_unique<VelocityBufferCameraBinder>();
        vbBinder->SetName("VelocityBufferCameraBinder");
        vbBinder->SetCamera3D(mainCamera3D_);
        velocityBufferCameraBinder_ = vbBinder.get();
        if (velocityBuffer_) vbBinder->AttachToRenderer(velocityBuffer_, "Object3D.Velocity");
        AddObject3D(std::move(vbBinder));
    }

    //==================================================
    // ↓ ここからゲームオブジェクト定義 ↓
    //==================================================

    // BPMシステムの追加
    {
        auto comp = std::make_unique<BPMSystem>(bpm_); // BPM で初期化
        bpmSystem_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // map (Box scaled)
    {
        for (int z = 0; z < kMapH; z++) {
            for (int x = 0; x < kMapW; x++) {

                auto obj = std::make_unique<Box>();
                obj->SetName("Map" ":x" + std::to_string(x) + ":z" + std::to_string(z));

				obj->RegisterComponent<BPMScaling>(mapScaleMin_, mapScaleMax_);

                if (auto* tr = obj->GetComponent3D<Transform3D>()) {
                    tr->SetTranslate(Vector3(2.0f * x, 0.0f, 2.0f * z));
                    tr->SetScale(Vector3(mapScaleMax_));
                }

                if (auto* mat = obj->GetComponent3D<Material3D>()) {
                    mat->SetTexture(TextureManager::GetTextureFromFileName("uvChecker.png"));
                }

                if (screenBuffer_)     obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
                if (shadowMapBuffer_)  obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
                if (velocityBuffer_)   obj->AttachToRenderer(velocityBuffer_, "Object3D.Velocity");

                // ここで “AddObject3D する前” にポインタ確保
                maps_[z][x] = obj.get();

                AddObject3D(std::move(obj));
            }
        }
    }

    // Player（衝突判定を修正）
    {
        auto modelData = ModelManager::GetModelDataFromFileName("Player.obj");
        auto obj = std::make_unique<Model>(modelData);
        obj->SetName("Player");
        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(0.0f, 1.0f, 0.0f));
            tr->SetScale(Vector3(playerScaleMax_));
        }

        obj->RegisterComponent<PlayerMove>(2.0f, playerMoveDuration_);
        if (auto* playerArrowMove = obj->GetComponent3D<PlayerMove>()) {
            playerArrowMove->SetInputCommand(GetInputCommand());
            playerArrowMove->SetBPMToleranceRange(playerBpmToleranceRange_);
        }

        obj->RegisterComponent<BPMScaling>(playerScaleMin_, playerScaleMax_);
        obj->RegisterComponent<Health>(10, 1.0f);

        // 衝突判定を追加（修正版）
        if (collider_ && collider_->GetCollider()) {
            ColliderInfo3D info;
            Math::AABB aabb;
            aabb.min = Vector3{ -0.75f, -0.75f, -0.75f };
            aabb.max = Vector3{ +0.75f, +0.75f, +0.75f };
            info.shape = aabb;
            info.attribute.set(0);  // Player属性を設定
            
            obj->RegisterComponent<Collision3D>(collider_->GetCollider(), info);
        }

        if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        if (velocityBuffer_) obj->AttachToRenderer(velocityBuffer_, "Object3D.Velocity");
        player_ = obj.get();
        AddObject3D(std::move(obj));
    }

	// ExplosionManager の追加
    {
        auto comp = std::make_unique<ExplosionManager>();
        comp->SetScreenBuffer(screenBuffer_);
        comp->SetShadowMapBuffer(shadowMapBuffer_);
		comp->SetCollider(collider_);
        comp->SetCollider2(collider_);
		comp->SetExplosionLifetime(explosionLifetime_);
        explosionManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    // BombManager の追加
    {
        auto comp = std::make_unique<BombManager>(bombMaxNumber_);
        comp->SetPlayer(player_);
        comp->SetScreenBuffer(screenBuffer_);
        comp->SetShadowMapBuffer(shadowMapBuffer_);
        comp->SetBPMToleranceRange(playerBpmToleranceRange_);
		comp->SetBombLifetimeBeats(bombLifetimeBeats_);
        comp->SetExplosionManager(explosionManager_);
        comp->SetInputCommand(GetInputCommand());
        comp->SetCollider(collider_);  // 追加
        bombManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    if (player_ && bombManager_) {
        if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
            playerMove->SetBombManager(bombManager_);
        }
    }

    // EnemyManagerの初期化（修正版）
    {
        auto comp = std::make_unique<EnemyManager>();
        comp->SetScreenBuffer(screenBuffer_);
        comp->SetShadowMapBuffer(shadowMapBuffer_);
        comp->SetBPMSystem(bpmSystem_);
        comp->SetMapSize(kMapW, kMapH);
        comp->SetCollider(collider_);
        comp->SetPlayer(player_);
        comp->SetBombManager(bombManager_);
        enemyManager_ = comp.get();
        AddSceneComponent(std::move(comp));
    }

    {
		auto comp = std::make_unique<EnemySpawner>(enemySpawnInterval_);
		comp->SetEnemyManager(enemyManager_);
		comp->SetBPMSystem(bpmSystem_);
		enemySpawner_ = comp.get();
		AddSceneComponent(std::move(comp));

        if (enemySpawner_) {
            constexpr float kTile = 2.0f;
            constexpr float kY = 1.0f;

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

    if (playBgm_) {
        auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Sounds/BPM120.wav");
        if (handle == AudioManager::kInvalidSoundHandle) {
            // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
            return;
        }
        AudioManager::Play(handle, 0.05f, 0.0f, true);
    }

    // Particles
    {
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

            if (screenBuffer_) obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            if (shadowMapBuffer_) obj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
            if (velocityBuffer_) obj->AttachToRenderer(velocityBuffer_, "Object3D.Velocity");

            auto* particlePtr = obj.get();
            AddObject3D(std::move(obj));

            auto lightObj = std::make_unique<PointLight>();
            lightObj->SetName("ParticlePointLight_" + std::to_string(i));
            lightObj->SetEnabled(true);
            lightObj->SetColor(particleLightColor_);
            lightObj->SetIntensity(0.0f);
            lightObj->SetRange(0.0f);

            auto* lightPtr = lightObj.get();
            if (screenBuffer_) lightObj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            AddObject3D(std::move(lightObj));

            particleLights_.push_back(ParticleLightPair{ particlePtr, lightPtr });
            lightManager->AddPointLight(lightPtr);
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

    // Player Health UI (ライフ表示)
    {
        auto comp = std::make_unique<PlayerHealthUI>(screenBuffer_);
        if (player_) {
            if (auto* health = player_->GetComponent3D<Health>()) {
                comp->SetHealth(health);
            }
			playerHealthUI_ = comp.get();
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
            //playerArrowMove->SetMoveDuration(bpmSystem_->GetBeatDuration());

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

    // テスト用のカメラ横移動処理
    {
        if (mainCamera3D_) {
            if (auto* tr = mainCamera3D_->GetComponent3D<Transform3D>()) {
                const float speed = 5.0f;
                static float angle = 0.0f;
                angle += speed * GetDeltaTime();
                Vector3 pos = tr->GetTranslate();
                pos.x = std::sin(angle) * 20.0f;
                tr->SetTranslate(pos);
            }
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

} // namespace KashipanEngine