#include "ExplosionManager.h"
#include "BombManager.h"
#include "ScoreDisplay.h"
#include "BombExplosionParticleManager.h"
#include "Objects/Components/Enemy/EnemyManager.h"
#include "Objects/Components/Player/ScoreManager.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Scenes/Components/PlayerHealthUI.h"
#include "objects/Components/Health.h"
#include "Scenes/Components/WaveSystem.h"
#include <algorithm>

namespace KashipanEngine {

ExplosionManager::ExplosionManager()
    : ISceneComponent("ExplosionManager") {
}

void ExplosionManager::Initialize() {
    // 初期化処理
    activeExplosions_.clear();
}

void ExplosionManager::Update() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    const float dt = GetDeltaTime();

    if (wallBreakParticleManager_) {
        wallBreakParticleManager_->SetParticleConfig(wallBreakConfig_);
    }

    // 爆発とボムの衝突をチェック
    CheckExplosionBombCollisions();

    // 爆発の更新と寿命管理（同時にスケールを徐々に小さくする）
    activeExplosions_.erase(
        std::remove_if(activeExplosions_.begin(), activeExplosions_.end(),
            [this, dt, ctx](ExplosionInfo& explosion) {
                explosion.elapsedTime += dt;

                // 経過割合（0..1）
                float t = 0.0f;
                if (explosionLifetime_ > 0.0f) {
                    t = explosion.elapsedTime / explosionLifetime_;
                    if (t < 0.0f) t = 0.0f;
                    if (t > 1.0f) t = 1.0f;
                } else {
                    t = 1.0f;
                }

                // 線形補間でスケールを縮小（初期 scale -> 0）
                Vector3 startScale((size_ * 2.0f) + 0.5f, 0.5f, 0.5f);
                Vector3 endScale(0.0f, 0.0f, 0.0f);
                Vector3 newScale = Vector3::Lerp(startScale, endScale, t);

                if (explosion.object) {
                    if (auto* tr = explosion.object->GetComponent3D<Transform3D>()) {
                        tr->SetScale(newScale);
                    }
                }

                // 線形補間でスケールを縮小（初期 scale -> 0）
                Vector3 startScale2(0.5f, 0.5f, (size_ * 2.0f) + 0.5f);
                Vector3 endScale2(0.0f, 0.0f, 0.0f);
                Vector3 newScale2 = Vector3::Lerp(startScale2, endScale2, t);

                if (explosion.object2) {
                    if (auto* tr = explosion.object2->GetComponent3D<Transform3D>()) {
                        tr->SetScale(newScale2);
                    }
                }

                // 寿命を超えた爆発を削除
                if (explosion.elapsedTime >= explosionLifetime_) {
                    if (explosion.object) {
                        ctx->RemoveObject3D(explosion.object);
                    }

                    if (explosion.object2) {
                        ctx->RemoveObject3D(explosion.object2);
                    }

                    return true;
                }

                return false;
            }),
        activeExplosions_.end()
    );
}

void ExplosionManager::SpawnExplosion(const Vector3& position, const float size) {
    auto* ctx = GetOwnerContext();
    if (!ctx || !screenBuffer_) {
        return;
    }

    // パーティクル生成
    if (bombExplosionParticleManager_) {
        bombExplosionParticleManager_->SpawnParticles(position, 10);
    }

    // 爆弾が起爆した瞬間にカメラをシェイク
    if (cameraController_) {
        cameraController_->Shake(shakePower_, shakeTime_);
    }

	size_ = size;
    size_ = std::clamp(size_, 1.0f, 1.0f);

    // 爆発範囲内の壁を破壊
    DestroyWallsInExplosionRange(position, size_);

    // 爆弾起爆位置に壁を設置
    CreateWallAtBombPosition(position);

    // 爆発オブジェクトを作成（BombManagerと同じパターン）
    auto modelData = ModelManager::GetModelDataFromFileName("Explosion.obj");
    auto explosion = std::make_unique<Model>(modelData);
    explosion->SetName("Explosion_" + std::to_string(activeExplosions_.size()));
    auto explosion2 = std::make_unique<Model>(modelData);
    explosion2->SetName("Explosion2_" + std::to_string(activeExplosions_.size()));

    // Transform設定
    if (auto* tr = explosion->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3((size_ * 2.0f) + 0.5f, 0.5f, 0.5f));
    }

    // Transform設定
    if (auto* tr = explosion2->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3(0.5f, 0.5f, (size_ * 2.0f) + 0.5f));
    }

    // Material設定
    if (auto* mat = explosion->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(false);
        mat->SetColor(Vector4(1.0f, 0.5f, 0.0f, 1.0f)); // オレンジ色の爆発エフェクト
    }

    if (auto* mat = explosion2->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(false);
        mat->SetColor(Vector4(1.0f, 0.5f, 0.0f, 1.0f)); // オレンジ色の爆発エフェクト
    }

    // 爆発オブジェクトのポインタを保存（move前）
    Model* explosionPtr = explosion.get();
    Model* explosion2Ptr = explosion2.get();

    // 新しい爆発情報を登録（先に登録して、コリジョンコールバックで参照できるようにする）
    ExplosionInfo info;
    info.object = explosionPtr;
    info.object2 = explosion2Ptr;
    info.elapsedTime = 0.0f;
    info.position = position;
    info.enemiesHit = 0;
    info.scoreCalculated = false;
    info.numberDisplayed = false;
    activeExplosions_.push_back(info);

    // 現在の爆発のインデックスを取得
    size_t currentExplosionIndex = activeExplosions_.size() - 1;

    if (collider_ && collider_->GetCollider()) {
        ColliderInfo3D collisionInfo;
        Math::AABB aabb;
        aabb.min = Vector3{ (-size_ / 2.0f), -0.5f, -0.5f };
        aabb.max = Vector3{ (+size_ / 2.0f), +0.5f, +0.5f };
        collisionInfo.shape = aabb;
        collisionInfo.attribute.set(2);

        // ラムダで爆発インデックスをキャプチャ
        collisionInfo.onCollisionEnter = [this, currentExplosionIndex](const HitInfo3D& hitInfo) {
            // 敵との衝突をEnemyManagerに通知（爆発の位置も渡す）
            if (enemyManager_ && currentExplosionIndex < activeExplosions_.size()) {
                const Vector3& explosionCenter = activeExplosions_[currentExplosionIndex].position;
                enemyManager_->OnExplosionHit(hitInfo.otherObject, explosionCenter);
            }

            // Playerと衝突したかチェック
            if (hitInfo.otherObject == player_) {
                // Playerを吹き飛ばす
                if (player_) {
                    if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
                        if (currentExplosionIndex < activeExplosions_.size()) {
                            const Vector3& explosionCenter = activeExplosions_[currentExplosionIndex].position;
                            playerMove->KnockBack(explosionCenter);
                        }
                    }
                }
            }
        };

        explosion->RegisterComponent<Collision3D>(collider_->GetCollider(), collisionInfo);
    }

    if (collider2_ && collider2_->GetCollider()) {
        ColliderInfo3D collisionInfo;
        Math::AABB aabb;
        aabb.min = Vector3{ -0.5f, -0.5f, (-size_ / 2.0f) };
        aabb.max = Vector3{ +0.5f, +0.5f, (+size_ / 2.0f) };
        collisionInfo.shape = aabb;
        collisionInfo.attribute.set(2);

        // ラムダで爆発インデックスをキャプチャ
        collisionInfo.onCollisionEnter = [this, currentExplosionIndex](const HitInfo3D& hitInfo) {
            // 敵との衝突をEnemyManagerに通知（爆発の位置も渡す）
            if (enemyManager_ && currentExplosionIndex < activeExplosions_.size()) {
                const Vector3& explosionCenter = activeExplosions_[currentExplosionIndex].position;
                enemyManager_->OnExplosionHit(hitInfo.otherObject, explosionCenter);
            }

            // Playerと衝突したかチェック
            if (hitInfo.otherObject == player_) {
                // Playerを吹き飛ばす
                if (player_) {
                    if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
                        if (currentExplosionIndex < activeExplosions_.size()) {
                            const Vector3& explosionCenter = activeExplosions_[currentExplosionIndex].position;
                            playerMove->KnockBack(explosionCenter);
                        }
                    }
                }
            }
        };


        explosion2->RegisterComponent<Collision3D>(collider2_->GetCollider(), collisionInfo);
    }

    // レンダラーにアタッチ
    if (screenBuffer_) {
        explosion->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        explosion2->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        explosion->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
        explosion2->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // シーンに追加
    ctx->AddObject3D(std::move(explosion));
    ctx->AddObject3D(std::move(explosion2));
}

void ExplosionManager::CreateWallAtBombPosition(const Vector3& position) {
    if (!walls_ || mapW_ <= 0 || mapH_ <= 0) return;

    // WaveSystemのパーティクル放出位置には壁を設置できない
    if (waveSystem_ && waveSystem_->IsParticleEmittingAt(position)) {
        return;
    }

    // 爆弾の位置をグリッド座標に変換
    const int bombX = static_cast<int>(std::round(position.x / 2.0f));
    const int bombZ = static_cast<int>(std::round(position.z / 2.0f));

    // 範囲チェック
    if (bombX < 0 || bombX >= mapW_ || bombZ < 0 || bombZ >= mapH_) return;

    // マップ中心の4か所には壁を生成しない
    const bool isCenterPosition = 
        (bombX == 4 && bombZ == 4) ||
        (bombX == 5 && bombZ == 4) ||
        (bombX == 4 && bombZ == 5) ||
        (bombX == 5 && bombZ == 5);
    
    if (isCenterPosition) {
        return;
    }

    // プレイヤーの位置をチェック
    if (player_) {
        if (auto* tr = player_->GetComponent3D<Transform3D>()) {
            const Vector3& playerPos = tr->GetTranslate();
            const int playerX = static_cast<int>(std::round(playerPos.x / 2.0f));
            const int playerZ = static_cast<int>(std::round(playerPos.z / 2.0f));
            
            // プレイヤーがいる位置には壁を起動しない
            if (bombX == playerX && bombZ == playerZ) {
                return;
            }
        }
    }

    // 1次元配列のインデックスを計算: walls_[z][x] = walls_[z * mapW + x]
    const int index = bombZ * mapW_ + bombX;

    // 再生成待機中の壁には設置できない
    if (walls_[index].isWaitingRespawn) {
        return;
    }

    // 壁をアクティブ化
    if (!walls_[index].isMoving) {
        walls_[index].moveTimer.Start(walls_[index].moveTime, false);
        walls_[index].isMoving = true;
		walls_[index].hp = 1;
    }
}

void ExplosionManager::DestroyWallsInExplosionRange(const Vector3& position, float size) {
    if (!walls_ || mapW_ <= 0 || mapH_ <= 0) return;

    if (isBreakWalls_) {

        // 爆発の中心位置をグリッド座標に変換
        const int centerX = static_cast<int>(std::round(position.x / 2.0f));
        const int centerZ = static_cast<int>(std::round(position.z / 2.0f));

        const int explosionSize = static_cast<int>(size);

        // X軸方向（左右）の壁を破壊
        for (int dx = -explosionSize; dx <= explosionSize; dx++) {
            const int targetX = centerX + dx;
            if (targetX >= 0 && targetX < mapW_ && centerZ >= 0 && centerZ < mapH_) {
                const int index = centerZ * mapW_ + targetX;
                if (walls_[index].isActive && walls_[index].isMoving) {
                    // 壁の中心位置を計算してパーティクルを生成
                    if (wallBreakParticleManager_) {
                        Vector3 wallCenter(targetX * 2.0f, 0.0f, centerZ * 2.0f);
                        wallBreakParticleManager_->SpawnParticles(wallCenter);
                    }

                    walls_[index].isActive = false;
                    walls_[index].isMoving = false;
                    walls_[index].moveTimer.Reset();
                    walls_[index].hp = 0;
                    walls_[index].isWaitingRespawn = true;
                    walls_[index].currentSpawnAgainCount = 0;
                }
            }
        }

        // Z軸方向（上下）の壁を破壊
        for (int dz = -explosionSize; dz <= explosionSize; dz++) {
            const int targetZ = centerZ + dz;
            if (centerX >= 0 && centerX < mapW_ && targetZ >= 0 && targetZ < mapH_) {
                const int index = targetZ * mapW_ + centerX;
                if (walls_[index].isActive && walls_[index].isMoving) {
                    // 壁の中心位置を計算してパーティクルを生成
                    if (wallBreakParticleManager_) {
                        Vector3 wallCenter(centerX * 2.0f, 0.0f, targetZ * 2.0f);
                        wallBreakParticleManager_->SpawnParticles(wallCenter);
                    }

                    walls_[index].isActive = false;
                    walls_[index].isMoving = false;
                    walls_[index].moveTimer.Reset();
                    walls_[index].hp = 0;
                    walls_[index].isWaitingRespawn = true;
                    walls_[index].currentSpawnAgainCount = 0;
                }
            }
        }

    }
}

void ExplosionManager::CheckExplosionBombCollisions() {
    if (!bombManager_) return;

    // 各爆発について、範囲内のボムをチェック
    for (const auto& explosion : activeExplosions_) {
        // 爆発の範囲（十字型の範囲）
        const float explosionRange = size_ * 2.0f;
        
        // BombManagerから全ボムの位置をチェックして起爆
        bombManager_->DetonateBombsInExplosionRange(explosion.position, explosionRange);
    }
}

bool ExplosionManager::IsWallActiveOrMoving(const Vector3& position) const {
    if (!walls_ || mapW_ <= 0 || mapH_ <= 0) {
        return false;
    }

    // 位置をグリッド座標に変換
    const int gridX = static_cast<int>(std::round(position.x / 2.0f));
    const int gridZ = static_cast<int>(std::round(position.z / 2.0f));

    // 範囲チェック
    if (gridX < 0 || gridX >= mapW_ || gridZ < 0 || gridZ >= mapH_) {
        return false;
    }

    // 1次元配列のインデックスを計算
    const int index = gridZ * mapW_ + gridX;

    // 壁がアクティブまたは移動中かをチェック
    return walls_[index].isActive || walls_[index].isMoving;
}

#if defined(USE_IMGUI)
void ExplosionManager::ShowImGui() {
    ImGui::Text("Active Explosions: %d", GetActiveExplosionCount());
    ImGui::DragFloat("Explosion Scale", &explosionScale_, 0.01f, 0.1f, 5.0f);
    ImGui::DragFloat("Explosion Lifetime", &explosionLifetime_, 0.01f, 0.1f, 10.0f);

    if (ImGui::TreeNode("Active Explosions List")) {
        for (size_t i = 0; i < activeExplosions_.size(); ++i) {
            const auto& explosion = activeExplosions_[i];
            ImGui::Text("Explosion %zu: Pos(%.2f, %.2f, %.2f) Time: %.2f/%.2f Enemies: %d",
                i,
                explosion.position.x, explosion.position.y, explosion.position.z,
                explosion.elapsedTime, explosionLifetime_,
                explosion.enemiesHit);
        }
        ImGui::TreePop();
    }
}
#endif

void ExplosionManager::ClearAllWalls() {
    if (!walls_ || mapW_ <= 0 || mapH_ <= 0) return;

    for (int z = 0; z < mapH_; z++) {
        for (int x = 0; x < mapW_; x++) {
            const int index = z * mapW_ + x;
            if (walls_[index].isActive) {
                walls_[index].isActive = false;
                walls_[index].isMoving = false;
                walls_[index].moveTimer.Reset();
                walls_[index].hp = 0;
                walls_[index].isWaitingRespawn = false;
                walls_[index].currentSpawnAgainCount = 0;
            }
        }
    }
}

} // namespace KashipanEngine
