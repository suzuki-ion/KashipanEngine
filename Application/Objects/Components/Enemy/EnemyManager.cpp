#include "EnemyManager.h"
#include "EnemyDieParticle.h"
#include "Objects/Components/BPMScaling.h"
#include "Objects/Components/Health.h"
#include "Objects/Components/3D/Collision3D.h"
#include "Scene/Components/ColliderComponent.h"
#include <algorithm>

namespace KashipanEngine {

void EnemyManager::Initialize() {
    ISceneComponent::Initialize();
    activeEnemies_.clear();
}

void EnemyManager::InitializeParticlePool() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    particlePool_.reserve(kParticlePoolSize_);

    // Boxモデルデータを取得
    auto boxModelData = ModelManager::GetModelDataFromFileName("MapBlock.obj");

    for (int i = 0; i < kParticlePoolSize_; ++i) {
        // パーティクルオブジェクトを生成
        auto particle = std::make_unique<Model>(boxModelData);
        particle->SetName("EnemyDieParticle_" + std::to_string(i));

        // Transform設定
        if (auto* tr = particle->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f }); // 初期は非表示
        }

        // Material設定 (オレンジ色)
        if (auto* mat = particle->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 1.0f, 0.3f, 0.0f, 1.0f });
            mat->SetEnableLighting(true);
        }

        // EnemyDieParticleコンポーネント追加
        EnemyDieParticle::ParticleConfig config;
        config.initialSpeed = 8.0f;
        config.speedVariation = 3.0f;
        config.lifeTimeSec = 0.8f;
        config.gravity = 15.0f;
        config.damping = 0.92f;
        config.baseScale = Vector3{ 1.0f, 1.0f, 1.0f };
        
        particle->RegisterComponent<EnemyDieParticle>(config);

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

void EnemyManager::Update() {
    if (!bpmSystem_) return;

    const int currentBeat = bpmSystem_->GetCurrentBeat();
    if (currentBeat == lastMoveBeat_) {
        CleanupDeadEnemies();
        return;
    }
    lastMoveBeat_ = currentBeat;

    // 1拍ごとに全敵を前進
    for (auto& e : activeEnemies_) {
        if (!e.object || e.isDead) continue;

        Vector3 delta{ 0.0f, 0.0f, 0.0f };
        switch (e.direction) {
        case EnemyDirection::Up:    delta = Vector3{ 0.0f, 0.0f,  moveDistance_ }; break;
        case EnemyDirection::Down:  delta = Vector3{ 0.0f, 0.0f, -moveDistance_ }; break;
        case EnemyDirection::Left:  delta = Vector3{ -moveDistance_, 0.0f, 0.0f }; break;
        case EnemyDirection::Right: delta = Vector3{  moveDistance_, 0.0f, 0.0f }; break;
        }

        Vector3 nextPos = e.position + delta;

        const float minX = -4.0f;
        const float minZ = -4.0f;
        const float maxX = static_cast<float>((mapW_ + 1) * 2.0f);
        const float maxZ = static_cast<float>((mapH_ + 1) * 2.0f);

        const bool out =
            (nextPos.x < minX) || (nextPos.x > maxX) ||
            (nextPos.z < minZ) || (nextPos.z > maxZ);

        if (out) {
            SpawnDieParticles(e.position);
            e.isDead = true;
            continue;
        }

        e.position = nextPos;

        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(nextPos);
        }
    }

    CleanupDeadEnemies();
}

void EnemyManager::SpawnEnemy(EnemyType type, EnemyDirection direction, const Vector3& position) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    // 敵オブジェクトを生成
    auto modelData = ModelManager::GetModelDataFromFileName("testEnemy.obj");
    auto enemy = std::make_unique<Model>(modelData);
    enemy->SetName("enemy_" + std::to_string(activeEnemies_.size()));

    if (auto* tr = enemy->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3(1.0f));
        switch (direction)
        {
        case EnemyDirection::Up:
            tr->SetRotate(Vector3{ 0.0f, 3.14f, 0.0f });
            break;
        case EnemyDirection::Down:
            tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });
            break;
        case EnemyDirection::Left:
            tr->SetRotate(Vector3{ 0.0f, 1.57f, 0.0f });
            break;
        case EnemyDirection::Right:
            tr->SetRotate(Vector3{ 0.0f, -1.57f, 0.0f });
            break;
        }
    }

    if (auto* mat = enemy->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // 敵オブジェクトのポインタを保存（move前）
    Object3DBase* enemyPtr = enemy.get();

    // 敵に衝突判定を追加
    if (collider_ && collider_->GetCollider()) {
        ColliderInfo3D collisionInfo;
        Math::AABB aabb;
        aabb.min = Vector3{ -0.75f, -0.75f, -0.75f };
        aabb.max = Vector3{ +0.75f, +0.75f, +0.75f };
        collisionInfo.shape = aabb;
        collisionInfo.attribute.set(1);  // Enemy属性

        // ラムダで敵オブジェクトポインタをキャプチャ
        collisionInfo.onCollisionEnter = [this, enemyPtr](const HitInfo3D& hitInfo) {
            // Playerと衝突したかチェック
            if (hitInfo.otherObject == player_) {
                // activeEnemies_から該当する敵を検索してisDead=trueに設定
                for (auto& e : activeEnemies_) {
                    if (e.object == enemyPtr) {
                        e.isDead = true;
                        break;
                    }
                }
                
                // Playerにダメージを与える
                if (player_) {
                    if (auto* health = player_->GetComponent3D<Health>()) {
                        health->Damage(1);
                    }
                }
            }

            // Bombと衝突したかチェック
            if (!hitInfo.otherObject) return;

            auto* otherCollision = hitInfo.otherObject->GetComponent3D<Collision3D>();
            if (!otherCollision) return;

            // Bomb属性（3）との衝突をチェック
            if (otherCollision->GetColliderInfo().attribute.test(3)) {
                // BombManagerに衝突を通知
                if (bombManager_) {
                    bombManager_->OnEnemyHit(hitInfo.otherObject);
                }
            }
        };

        enemy->RegisterComponent<Collision3D>(collider_->GetCollider(), collisionInfo);
    }

    enemy->RegisterComponent<BPMScaling>(0.8f, 1.0f);

    // レンダラーにアタッチ
    if (screenBuffer_) {
        enemy->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        enemy->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // 敵情報を登録
    EnemyInfo info;
    info.object = enemyPtr;
    info.type = type;
    info.direction = direction;
    info.position = position;
    activeEnemies_.push_back(info);

    // シーンに追加
    ctx->AddObject3D(std::move(enemy));
}

void EnemyManager::SpawnDieParticles(const Vector3& position) {
    // プールから非アクティブなパーティクルを探して再利用
    int particlesSpawned = 0;
    const int particlesToSpawn = 15; // 1回の死亡で発生させるパーティクル数

    for (auto* particle : particlePool_) {
        if (particlesSpawned >= particlesToSpawn) break;

        auto* dieParticle = particle->GetComponent3D<EnemyDieParticle>();

        dieParticle->Spawn(position);
        particlesSpawned++;
    }
}

void EnemyManager::CleanupDeadEnemies() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    for (size_t i = 0; i < activeEnemies_.size();) {
        auto& e = activeEnemies_[i];

        if (e.isDead && e.object) {
            ctx->RemoveObject3D(e.object);
            activeEnemies_.erase(activeEnemies_.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void EnemyManager::OnExplosionHit(Object3DBase* hitObject) {
    if (!hitObject) return;

    // activeEnemies_からも削除フラグを立てる
    for (auto& enemyInfo : activeEnemies_) {
        if (enemyInfo.object == hitObject) {
            SpawnDieParticles(enemyInfo.position);
            enemyInfo.isDead = true;

            break;
        }
    }
}

} // namespace KashipanEngine