#include "EnemySpawner.h"
#include <random>
#include <algorithm>

namespace KashipanEngine {

    void EnemySpawner::Initialize() {
        // 初期化処理
        lastSpawnBeat_ = -1;
        preSpawnBeat_ = -1;
        isEmittingParticles_ = false;
    }

    void EnemySpawner::InitializeParticlePool(int particlesPerFrame) {
        auto* ctx = GetOwnerContext();
        if (!ctx) return;

        particlesPerFrame_ = particlesPerFrame;
        particlePool_.reserve(kParticlePoolSize_);

        for (int i = 0; i < kParticlePoolSize_; ++i) {
            // パーティクルオブジェクトを生成
            auto particle = std::make_unique<Box>();
            particle->SetName("EnemySpawnParticle_" + std::to_string(i));

            // Transform設定
            if (auto* tr = particle->GetComponent3D<Transform3D>()) {
                tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f }); // 初期は非表示
            }

            // Material設定 (赤色)
            if (auto* mat = particle->GetComponent3D<Material3D>()) {
                mat->SetColor(Vector4{ 0.8f, 0.8f, 0.8f, 1.0f });
                mat->SetEnableLighting(true);
            }

            // EnemySpawnParticleコンポーネント追加
            particleConfig_.initialSpeed = 5.0f;
            particleConfig_.speedVariation = 2.0f;
            particleConfig_.lifeTimeSec = 2.0f;
            particleConfig_.gravity = -5.0f;
            particleConfig_.damping = 0.95f;
            particleConfig_.spreadAngle = 0.0f;
            particleConfig_.baseScale = Vector3{ 0.3f, 0.3f, 0.3f };

            particle->RegisterComponent<EnemySpawnParticle>(particleConfig_);

            // レンダラーにアタッチ
            if (screenBuffer_) {
                particle->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }
            if (shadowMapBuffer_) {
                particle->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
            }

            particlePool_.push_back(particle.get());
            ctx->AddObject3D(std::move(particle));
        }
    }

    void EnemySpawner::Update() {
        if (!bpmSystem_ || !enemyManager_) return;

        int currentBeat = bpmSystem_->GetCurrentBeat();

        // プール内の全パーティクルにconfigを適用
        for (auto* particle : particlePool_) {
            if (auto* spawnParticle = particle->GetComponent3D<EnemySpawnParticle>()) {
                spawnParticle->SetConfig(particleConfig_);
            }
        }

        // スポーン3拍前の検出
        if (currentBeat != preSpawnBeat_ && (currentBeat + 3) % spawnInterval_ == 0) {
            preSpawnBeat_ = currentBeat;
            
            // 次のスポーン位置を決定
            const auto& spawnPoint = SelectSpawnPoint();
            nextSpawnPosition_ = spawnPoint.position;
            
            // パーティクル放出開始
            isEmittingParticles_ = true;
        }

        // パーティクルを毎フレーム放出（プールから再利用）
        if (isEmittingParticles_) {
            SpawnParticlesAtPosition(nextSpawnPosition_);
        }

        // 実際の敵のスポーン
        if (currentBeat != lastSpawnBeat_ && currentBeat % spawnInterval_ == 0) {
            lastSpawnBeat_ = currentBeat;
            SpawnEnemy();
            
            // パーティクル放出停止
            isEmittingParticles_ = false;
        }
    }

    void EnemySpawner::SpawnEnemy() {
        if (spawnPoints_.empty()) return;

        // 既に決定されている位置を使用（パーティクルと同じ位置）
        Vector3 spawnPos = nextSpawnPosition_;
        
        EnemyType type = DetermineEnemyType();
        EnemyDirection direction = ChooseSpawnDirection_NoOutward(spawnPos);

        // EnemyManager::SpawnEnemy のシグネチャに合わせて修正
        enemyManager_->SpawnEnemy(type, direction, spawnPos);
    }

    void EnemySpawner::SpawnParticlesAtPosition(const Vector3& position) {
        // プールから非アクティブなパーティクルを探して再利用
        int particlesSpawned = 0;

        for (auto* particle : particlePool_) {
            if (particlesSpawned >= particlesPerFrame_) break;

            auto* spawnParticle = particle->GetComponent3D<EnemySpawnParticle>();
            if (!spawnParticle) continue;

            // 非アクティブなパーティクルのみ再利用
            if (!spawnParticle->IsAlive()) {
                spawnParticle->Spawn(position);
                particlesSpawned++;
            }
        }
    }

    void EnemySpawner::AddSpawnPoint(const Vector3& position) {
        spawnPoints_.push_back({ position });
    }

    EnemyType EnemySpawner::DetermineEnemyType() const {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, static_cast<int>(EnemyType::Count) - 1);

        return static_cast<EnemyType>(dis(gen));
    }

    EnemyDirection EnemySpawner::ChooseSpawnDirection_NoOutward(const Vector3& pos, int mapW, int mapH, float tile) const {
        // マップの中心を計算
        float centerX = (mapW - 1) * tile * 0.5f;
        float centerZ = (mapH - 1) * tile * 0.5f;

        // スポーン位置から中心への方向を計算
        Vector3 toCenter = { centerX - pos.x, 0.0f, centerZ - pos.z };
        toCenter = toCenter.Normalize();

        // 最も近い方向を選択
        float dotUp = toCenter.Dot({ 0.0f, 0.0f, 1.0f });
        float dotDown = toCenter.Dot({ 0.0f, 0.0f, -1.0f });
        float dotLeft = toCenter.Dot({ -1.0f, 0.0f, 0.0f });
        float dotRight = toCenter.Dot({ 1.0f, 0.0f, 0.0f });

        float maxDot = std::max({ dotUp, dotDown, dotLeft, dotRight });

        if (maxDot == dotUp) return EnemyDirection::Up;
        if (maxDot == dotDown) return EnemyDirection::Down;
        if (maxDot == dotLeft) return EnemyDirection::Left;
        return EnemyDirection::Right;
    }

    const SpawnPoint& EnemySpawner::SelectSpawnPoint() const {
        // 重み付きランダム選択（簡易版）
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0, spawnPoints_.size() - 1);
        
        return spawnPoints_[dis(gen)];
    }

#if defined(USE_IMGUI)
    void EnemySpawner::ShowImGui() {
        if (ImGui::TreeNode("EnemySpawner")) {
            ImGui::Text("Spawn Points: %zu", spawnPoints_.size());
            ImGui::DragInt("Spawn Interval", &spawnInterval_, 1, 1, 16);
            ImGui::Text("Last Spawn Beat: %d", lastSpawnBeat_);
            ImGui::Text("Pre Spawn Beat: %d", preSpawnBeat_);
            ImGui::DragInt("Particles Per Frame", &particlesPerFrame_, 1, 1, 10);
            ImGui::Checkbox("Is Emitting Particles", &isEmittingParticles_);
            
            if (ImGui::TreeNode("Particle Config")) {
                ImGui::DragFloat("Initial Speed", &particleConfig_.initialSpeed, 0.1f, 0.0f, 20.0f);
                ImGui::DragFloat("Speed Variation", &particleConfig_.speedVariation, 0.1f, 0.0f, 10.0f);
                ImGui::DragFloat("Life Time (sec)", &particleConfig_.lifeTimeSec, 0.01f, 0.1f, 5.0f);
                ImGui::DragFloat("Gravity", &particleConfig_.gravity, 0.1f, -20.0f, 0.0f);
                ImGui::DragFloat("Damping", &particleConfig_.damping, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat3("Base Scale", &particleConfig_.baseScale.x, 0.1f, 0.0f, 10.0f);
                ImGui::TreePop();
            }
            
            ImGui::TreePop();
        }
    }
#endif

} // namespace KashipanEngine