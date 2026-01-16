#include "EnemyManager.h"
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
            if (hitInfo.otherObject != player_) return;

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

    // activeEnemies_から該当する敵を検索してisDead=trueに設定
    for (auto& e : activeEnemies_) {
        if (e.object == hitObject) {
            e.isDead = true;
            break;
        }
    }
}

} // namespace KashipanEngine