#include "EnemySpawner.h"
#include <random>
#include <algorithm>

namespace KashipanEngine {

    void EnemySpawner::Initialize() {
        // 初期化処理
        lastSpawnBeat_ = -1;
        preSpawnBeat_ = -1;
        particlesSpawned_ = false;
        isEmittingParticles_ = false;
    }

    void EnemySpawner::Update() {
        if (!bpmSystem_ || !enemyManager_) return;

        int currentBeat = bpmSystem_->GetCurrentBeat();

        // スポーン3拍前の検出
        if (currentBeat != preSpawnBeat_ && (currentBeat + 3) % spawnInterval_ == 0) {
            preSpawnBeat_ = currentBeat;
            
            // 次のスポーン位置を決定
            const auto& spawnPoint = SelectSpawnPoint();
            nextSpawnPosition_ = spawnPoint.position;
            
            // パーティクル放出開始
            isEmittingParticles_ = true;
        }

        // パーティクルを毎フレーム放出
        if (isEmittingParticles_) {
            SpawnParticlesAtPosition(nextSpawnPosition_);
        }

        // 実際の敵のスポーン
        if (currentBeat != lastSpawnBeat_ && currentBeat % spawnInterval_ == 0) {
            lastSpawnBeat_ = currentBeat;
            SpawnEnemy();
            
            // パーティクル放出停止
            isEmittingParticles_ = false;
            particlesSpawned_ = false;
        }

        // 消滅したパーティクルオブジェクトを削除
        auto* ctx = GetOwnerContext();
        particleObjects_.erase(
            std::remove_if(particleObjects_.begin(), particleObjects_.end(),
                [ctx](Object3DBase* obj) {
                    auto* particle = obj->GetComponent3D<EnemySpawnParticle>();
                    if (particle && !particle->IsAlive()) {
                        // レンダラーからデタッチ
                        obj->DetachFromRenderer();
                        
                        // コンテキストから削除
                        if (ctx) {
                            ctx->RemoveObject3D(obj);
                        }
                        return true;
                    }
                    return false;
                }),
            particleObjects_.end()
        );
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
        auto* ctx = GetOwnerContext();
        if (!ctx) return;

        // Boxモデルデータを取得
        auto boxModelData = ModelManager::GetModelDataFromFileName("MapBlock.obj");

        // 毎フレーム少量のパーティクルを生成（例: 1~3個）
        constexpr int particlesPerFrame = 1;

        for (int i = 0; i < particlesPerFrame; ++i) {
            auto particleObj = std::make_unique<Model>(boxModelData);
            particleObj->SetName("EnemySpawnParticle_" + std::to_string(particleObjects_.size()));

            // Transform3Dコンポーネントを追加
            auto* transform = particleObj->GetComponent3D<Transform3D>();
            if (!transform) {
                particleObj->RegisterComponent<Transform3D>();
                transform = particleObj->GetComponent3D<Transform3D>();
            }
            
            if (transform) {
                transform->SetTranslate(position);
                transform->SetScale(particleConfig_.baseScale);
            }

            // EnemySpawnParticleコンポーネントを追加
            particleObj->RegisterComponent<EnemySpawnParticle>(particleConfig_);
            auto* particle = particleObj->GetComponent3D<EnemySpawnParticle>();
            if (particle) {
                particle->Spawn(position);
            }

            // レンダラーにアタッチ
            if (screenBuffer_) {
                particleObj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
            }
            if (shadowMapBuffer_) {
                particleObj->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
            }

            particleObjects_.push_back(particleObj.get());
            ctx->AddObject3D(std::move(particleObj));
        }
    }

    void EnemySpawner::AddSpawnPoint(const Vector3& position) {
        spawnPoints_.push_back({ position });
    }

    EnemyType EnemySpawner::DetermineEnemyType() const {
        //static std::random_device rd;
        //static std::mt19937 gen(rd());
        //std::uniform_int_distribution<size_t> dis(0, spawnPoints_.size() - 1);

        return EnemyType::Basic;
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
            ImGui::Text("Active Particles: %zu", particleObjects_.size());
            ImGui::Checkbox("Is Emitting Particles", &isEmittingParticles_);
            
            if (ImGui::TreeNode("Particle Config")) {
                ImGui::DragInt("Particle Count", &particleConfig_.particleCount, 1, 1, 50);
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