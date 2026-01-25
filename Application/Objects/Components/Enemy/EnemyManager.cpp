#include "EnemyManager.h"
#include "EnemyDieParticle.h"
#include "Objects/Components/BPMScaling.h"
#include "Objects/Components/Health.h"
#include "Objects/Components/3D/Collision3D.h"
#include "Scene/Components/ColliderComponent.h"
#include <algorithm>

#include "Utilities/Easing.h"

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

    // BPM進行度を取得して更新
    bpmProgress_ = bpmSystem_->GetBeatProgress();

    const int currentBeat = bpmSystem_->GetCurrentBeat();
    
    // 新しい拍になったかチェック
    bool isNewBeat = (currentBeat != lastMoveBeat_);
    
    if (isNewBeat) {
        lastMoveBeat_ = currentBeat;
        
        for (auto& e : activeEnemies_) {

            // 移動する拍かどうかを判定
            bool isMoveBeat = (currentBeat % e.moveEveryNBeats == 0);

            if (!e.object || e.isDead) continue;

            if (isMoveBeat) {
                // 移動する拍: 目標位置を計算
                Vector3 delta{ 0.0f, 0.0f, 0.0f };
                switch (e.direction) {
                case EnemyDirection::Up:    delta = Vector3{ 0.0f, 0.0f,  moveDistance_ }; break;
                case EnemyDirection::Down:  delta = Vector3{ 0.0f, 0.0f, -moveDistance_ }; break;
                case EnemyDirection::Left:  delta = Vector3{ -moveDistance_, 0.0f, 0.0f }; break;
                case EnemyDirection::Right: delta = Vector3{  moveDistance_, 0.0f, 0.0f }; break;
                }

                // 開始位置を現在位置に、目標位置を計算
                e.startPosition = e.position;
                e.targetPosition = e.position + delta;
            } else {
                // 止まる拍: その場でジャンプのみ（開始と目標を同じにする）
                e.startPosition = e.position;
                e.targetPosition = e.position;
            }
        }
    }

    // 毎フレーム、イージングで位置を更新
    for (auto& e : activeEnemies_) {
        if (!e.object || e.isDead) continue;

		e.object->GetComponent3D<BPMScaling>()->SetBPMProgress(bpmProgress_);

        // イージングで現在位置を計算（移動する拍では移動、止まる拍では同じ位置）
        Vector3 nextPos = Vector3(MyEasing::Lerp(e.startPosition, e.targetPosition, bpmProgress_, EaseType::EaseOutQuint));
        float currentPosY = float(MyEasing::Lerp_GAB(0.0f, 0.5f, bpmProgress_, EaseType::EaseOutCirc, EaseType::EaseInCirc));

        const float minX = -2.0f;
        const float minZ = -2.0f;
        const float maxX = static_cast<float>((mapW_) * 2.0f);
        const float maxZ = static_cast<float>((mapH_) * 2.0f);

        // 目標位置が範囲外かチェック
        const bool out =
            (e.targetPosition.x <= minX) || (e.targetPosition.x >= maxX) ||
            (e.targetPosition.z <= minZ) || (e.targetPosition.z >= maxZ);

        e.position = nextPos;

        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(Vector3(nextPos.x, currentPosY, nextPos.z));
        }

        // 拍の終わり（進行度が1.0に近い）で範囲外なら削除
        if (out && bpmProgress_ > 0.5f) {
            SpawnDieParticles(e.targetPosition);
            e.isDead = true;
        }
    }

    CleanupDeadEnemies();
}

void EnemyManager::SpawnEnemy(EnemyType type, EnemyDirection direction, const Vector3& position) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    // 敵オブジェクトを生成
    auto modelData = ModelManager::GetModelDataFromFileName("enemy.obj");
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
        switch (type) {
        case EnemyType::Basic:  mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); break;
        case EnemyType::Speedy: mat->SetColor(Vector4(0.0f, 0.0f, 1.0f, 1.0f)); break;
        }
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

    enemy->RegisterComponent<BPMScaling>(Vector3(1.2f ,0.5f ,1.2f), Vector3(1.0f ,1.0f ,1.0f),EaseType::EaseOutExpo);

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
    info.startPosition = position;
    info.targetPosition = position;

    switch (type) {
    case EnemyType::Basic:  info.moveEveryNBeats = 2; break; 
    case EnemyType::Speedy: info.moveEveryNBeats = 1; break;
    }

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
        if (!dieParticle) continue;

        // 非アクティブなパーティクルのみ再利用
        if (!dieParticle->IsAlive()) {
            dieParticle->Spawn(position);
            particlesSpawned++;
        }
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