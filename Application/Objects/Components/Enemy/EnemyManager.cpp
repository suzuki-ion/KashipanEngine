#include "EnemyManager.h"
#include "EnemyDieParticle.h"
#include "Objects/Components/BPMScaling.h"
#include "Objects/Components/Health.h"
#include "Objects/Components/Player/PlayerMove.h"
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

    for (int i = 0; i < kParticlePoolSize_; ++i) {
        // パーティクルオブジェクトを生成
        auto particle = std::make_unique<Box>();
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
        ParticleConfig config;
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
    const float dt = GetDeltaTime();
    
    // 新しい拍になったかチェック
    bool isNewBeat = (currentBeat != lastMoveBeat_);
    
    if (isNewBeat) {
        lastMoveBeat_ = currentBeat;
        
        for (auto& e : activeEnemies_) {
            if (!e.object || e.isDead) continue;
            
            // 吹き飛び中の敵は通常の移動をスキップ
            if (e.isKnockedBack) continue;

            // 前の拍の移動を完了: targetPositionをpositionに反映
            e.position = e.targetPosition;

            // 移動する拍かどうかを判定
            bool isMoveBeat = (currentBeat % e.moveEveryNBeats == 0);

            if (isMoveBeat) {
                Vector3 delta{ 0.0f, 0.0f, 0.0f };
                
                // 中心エリアへの移動中かチェック
                if (e.isMovingToCenter) {
                    // 中心エリア内の最も近い中心位置を選択
                    Vector3 centerTarget = Vector3(9.0f, 0.0f, 9.0f);  // 中心エリアの中央（グリッド座標4.5, 4.5）
                    
                    // 現在位置から中心への方向を計算
                    Vector3 toCenter = centerTarget - e.position;
                    
                    // X軸とZ軸のどちらの距離が大きいかで移動方向を決定
                    if (std::abs(toCenter.x) > std::abs(toCenter.z)) {
                        // X軸方向に移動
                        delta = Vector3{ toCenter.x > 0.0f ? moveDistance_ : -moveDistance_, 0.0f, 0.0f };
                        
                        // モデルの向きを更新
                        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
                            if (toCenter.x > 0.0f) {
                                tr->SetRotate(Vector3{ 0.0f, -1.57f, 0.0f });  // 右向き
                            } else {
                                tr->SetRotate(Vector3{ 0.0f, 1.57f, 0.0f });   // 左向き
                            }
                        }
                    } else {
                        // Z軸方向に移動
                        delta = Vector3{ 0.0f, 0.0f, toCenter.z > 0.0f ? moveDistance_ : -moveDistance_ };
                        
                        // モデルの向きを更新
                        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
                            if (toCenter.z > 0.0f) {
                                tr->SetRotate(Vector3{ 0.0f, 3.14f, 0.0f });   // 上向き
                            } else {
                                tr->SetRotate(Vector3{ 0.0f, 0.0f, 0.0f });    // 下向き
                            }
                        }
                    }
                } else {
                    // 通常の方向移動
                    switch (e.direction) {
                    case EnemyDirection::Up:    delta = Vector3{ 0.0f, 0.0f,  moveDistance_ }; break;
                    case EnemyDirection::Down:  delta = Vector3{ 0.0f, 0.0f, -moveDistance_ }; break;
                    case EnemyDirection::Left:  delta = Vector3{ -moveDistance_, 0.0f, 0.0f }; break;
                    case EnemyDirection::Right: delta = Vector3{  moveDistance_, 0.0f, 0.0f }; break;
                    }
                }

                // 移動先の座標を計算（前の移動が完了したpositionを基準にする）
                Vector3 nextPosition = e.position + delta;
                int nextX = static_cast<int>(std::round(nextPosition.x / 2.0f));
                int nextZ = static_cast<int>(std::round(nextPosition.z / 2.0f));

                // 十字エリアに入ったかチェック
                bool reachedCrossArea = IsCrossAreaFromCenter(nextX, nextZ);
                
                // 十字エリアに入ったら移動モードを変更（反転フラグが立っていない場合のみ）
                if (reachedCrossArea && !e.isMovingToCenter && !e.isReversedFromWall) {
                    e.isMovingToCenter = true;
                }

                // 中心エリアに入ったかチェック
                bool reachedCenterArea = false;
                for (const auto& centerPos : centerPositions_) {
                    if (nextX == centerPos.first && nextZ == centerPos.second) {
                        reachedCenterArea = true;
                        break;
                    }
                }
                
                // 中心エリアに入ったら移動モードを変更（反転フラグが立っていない場合のみ）
                if (reachedCenterArea && !e.isMovingToCenter && !e.isReversedFromWall) {
                    e.isMovingToCenter = true;
                }

                // 移動先に壁があるかチェック
                if (IsWallAt(nextX, nextZ)) {
                    // 壁がある場合は移動せず、その場に留まり、方向を逆転させる
                    e.startPosition = e.position;
                    e.targetPosition = e.position;
                    
                    // 壁にダメージを与える
                    DamageWallAt(nextX, nextZ);
                    
                    // 中心移動中も壁に当たったら方向を逆転
                    if (e.isMovingToCenter) {
                        // 反転フラグを立てる
                        e.isReversedFromWall = true;
                        // 中心移動モードを解除
                        e.isMovingToCenter = false;
                        
                        // 中心移動中の逆転: 中心から離れる方向に移動
                        Vector3 centerTarget = Vector3(9.0f, 0.0f, 9.0f);
                        Vector3 fromCenter = e.position - centerTarget;
                        
                        // 逆方向をdirectionに設定
                        if (std::abs(fromCenter.x) > std::abs(fromCenter.z)) {
                            // X軸方向
                            if (fromCenter.x > 0.0f) {
                                e.direction = EnemyDirection::Right;
                            } else {
                                e.direction = EnemyDirection::Left;
                            }
                        } else {
                            // Z軸方向
                            if (fromCenter.z > 0.0f) {
                                e.direction = EnemyDirection::Up;
                            } else {
                                e.direction = EnemyDirection::Down;
                            }
                        }
                        
                        // モデルの向きを更新
                        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
                            switch (e.direction) {
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
                    } else {
                        // 通常の方向転換
                        switch (e.direction) {
                        case EnemyDirection::Up:    e.direction = EnemyDirection::Down; break;
                        case EnemyDirection::Down:  e.direction = EnemyDirection::Up; break;
                        case EnemyDirection::Left:  e.direction = EnemyDirection::Right; break;
                        case EnemyDirection::Right: e.direction = EnemyDirection::Left; break;
                        }
                        
                        // 方向転換に合わせてオブジェクトの向きも更新
                        if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
                            switch (e.direction) {
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
                    }
                } else {
                    // 壁がない場合は通常通り移動
                    e.startPosition = e.position;
                    e.targetPosition = nextPosition;
                }
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

        // 吹き飛び処理
        if (e.isKnockedBack) {
            // 現在の位置を保存
            Vector3 oldPosition = e.position;
            
            // 吹き飛び速度を適用（減衰なし）
            e.position += e.knockbackVelocity * dt;
            
            // 吹き飛び中の壁破壊チェック
            const int currentGridX = static_cast<int>(std::round(e.position.x / 2.0f));
            const int currentGridZ = static_cast<int>(std::round(e.position.z / 2.0f));
            
            // 壁があれば破壊
            if (IsWallAt(currentGridX, currentGridZ)) {
                DestroyWallAt(currentGridX, currentGridZ);
            }
            
            // 吹き飛び中は現在位置をそのまま描画
            if (auto* tr = e.object->GetComponent3D<Transform3D>()) {
                tr->SetTranslate(e.position);
            }
            
            // マップ範囲外チェック
            const float minX = -2.0f;
            const float minZ = -2.0f;
            const float maxX = static_cast<float>((mapW_) * 2.0f);
            const float maxZ = static_cast<float>((mapH_) * 2.0f);
            
            if (e.position.x <= minX || e.position.x >= maxX ||
                e.position.z <= minZ || e.position.z >= maxZ) {
                SpawnDieParticles(e.position);
                e.isDead = true;
            }
            
            continue;
        }

        // 通常の移動処理（吹き飛び中でない場合）
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

        // マップ中心に到達したかチェック
        const int targetGridX = static_cast<int>(std::round(e.targetPosition.x / 2.0f));
        const int targetGridZ = static_cast<int>(std::round(e.targetPosition.z / 2.0f));
        
        bool reachedCenter = false;
        for (const auto& centerPos : centerPositions_) {
            if (targetGridX == centerPos.first && targetGridZ == centerPos.second) {
                reachedCenter = true;
                break;
            }
        }
        
        // 中心に到達した場合、プレイヤーにダメージを与えて敵を削除
        if (reachedCenter && bpmProgress_ > 0.5f && e.isMovingToCenter) {
            if (player_) {
                if (auto* health = player_->GetComponent3D<Health>()) {
                    health->Damage(1);
                }
            }
            SpawnDieParticles(e.targetPosition);
            e.isDead = true;
        }

        // 描画位置のみ更新（e.positionは次の拍まで更新しない）
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

	std::string modelPath;
    switch (type)
    {
    case EnemyType::Basic:
		modelPath = "enemy_Normal.obj";
        break;
    case EnemyType::Speedy:
		modelPath = "enemy_Speed.obj";
        break;
    }

    auto modelData = ModelManager::GetModelDataFromFileName(modelPath);
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
            // この敵が吹き飛び中かどうかをチェック
            bool isThisEnemyKnockedBack = false;
            for (const auto& e : activeEnemies_) {
                if (e.object == enemyPtr && e.isKnockedBack) {
                    isThisEnemyKnockedBack = true;
                    break;
                }
            }
            
            // 吹き飛び中の敵が他の敵と衝突した場合、相手を破壊
            if (isThisEnemyKnockedBack) {
                auto* otherCollision = hitInfo.otherObject ? hitInfo.otherObject->GetComponent3D<Collision3D>() : nullptr;
                if (otherCollision && otherCollision->GetColliderInfo().attribute.test(1)) {
                    // 相手も敵なので破壊
                    for (auto& e : activeEnemies_) {
                        if (e.object == hitInfo.otherObject && !e.isKnockedBack) {
                            SpawnDieParticles(e.position);
                            e.isDead = true;
                            break;
                        }
                    }
                }
                
                // 吹き飛び中の敵がプレイヤーと衝突した場合、プレイヤーを吹き飛ばす
                if (hitInfo.otherObject == player_) {
                    if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
                        // 敵の位置を爆発中心として使用
                        for (const auto& e : activeEnemies_) {
                            if (e.object == enemyPtr) {
                                playerMove->KnockBack(e.position);
                                break;
                            }
                        }
                    }
                }
                
                return;
            }
            
            // Playerと衝突したかチェック（通常の敵の場合はダメージを与える）
            if (hitInfo.otherObject == player_) {
                // activeEnemies_から該当する敵を検索してisDead=trueに設定
                //for (auto& e : activeEnemies_) {
                //    if (e.object == enemyPtr) {
                //        e.isDead = true;
                //        break;
                //    }
                //}
                
                if (auto* playerMove = player_->GetComponent3D<PlayerMove>()) {
                    // 敵の位置を爆発中心として使用
                    for (const auto& e : activeEnemies_) {
                        if (e.object == enemyPtr) {
                            playerMove->KnockBack(e.position);
                            break;
                        }
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

    auto handle = AudioManager::GetSoundHandleFromAssetPath("Application/Audio/InGame/enemyDeath.mp3");
    if (handle == AudioManager::kInvalidSoundHandle) {
        // 音声が未ロードならログ出力するか無視（ここでは無害に戻す）
        return;
    }
    AudioManager::Play(handle, dieVolume_);

    for (auto* particle : particlePool_) {
        if (particlesSpawned >= particlesToSpawn) break;

        auto* dieParticle = particle->GetComponent3D<EnemyDieParticle>();
        if (!dieParticle) continue;

        // 非アクティブなパーティクルのみ再利用
        if (!dieParticle->IsAlive()) {
            dieParticle->SetConfig(dieParticleConfig_);
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

void EnemyManager::OnExplosionHit(Object3DBase* hitObject, const Vector3& explosionCenter) {
    if (!hitObject) return;

    // activeEnemies_から該当する敵を探して吹き飛ばす
    for (auto& enemyInfo : activeEnemies_) {
        if (enemyInfo.object == hitObject) {
            
            // 既に吹き飛び中なら無視
            if (enemyInfo.isKnockedBack) {
                return;
            }
            
            // 移動中の場合、現在の描画位置を取得して論理位置として設定
            if (auto* tr = enemyInfo.object->GetComponent3D<Transform3D>()) {
                Vector3 currentVisualPos = tr->GetTranslate();
                currentVisualPos.y = 0.0f; // Y軸はリセット
                enemyInfo.position = currentVisualPos;
                enemyInfo.startPosition = currentVisualPos;
                enemyInfo.targetPosition = currentVisualPos;
            }
            
            // 爆発の中心から敵への方向ベクトルを計算
            Vector3 toEnemy = enemyInfo.position - explosionCenter;
            toEnemy.y = 0.0f; // Y軸は無視
            
            // ベクトルを正規化（長さを1にする）
            Vector3 knockbackDirection = toEnemy.Normalize();
            
            // 正規化に失敗した場合（敵と爆発が完全に同じ位置）は、敵の進行方向の逆を使用
            if (knockbackDirection.Length() < 0.01f) {
                switch (enemyInfo.direction) {
                case EnemyDirection::Up:    
                    knockbackDirection = Vector3{ 0.0f, 0.0f, -1.0f };
                    break;
                case EnemyDirection::Down:  
                    knockbackDirection = Vector3{ 0.0f, 0.0f, 1.0f };
                    break;
                case EnemyDirection::Left:  
                    knockbackDirection = Vector3{ 1.0f, 0.0f, 0.0f };
                    break;
                case EnemyDirection::Right: 
                    knockbackDirection = Vector3{ -1.0f, 0.0f, 0.0f };
                    break;
                }
            } else {
                // 方向ベクトルを最も近い軸方向（上下左右）にスナップ
                float absX = std::abs(knockbackDirection.x);
                float absZ = std::abs(knockbackDirection.z);
                
                // X軸とZ軸のどちらが強いかを判定
                if (absX > absZ) {
                    // X軸方向に吹き飛ぶ（左右）
                    if (knockbackDirection.x > 0.0f) {
                        knockbackDirection = Vector3{ 1.0f, 0.0f, 0.0f };  // 右
                    } else {
                        knockbackDirection = Vector3{ -1.0f, 0.0f, 0.0f }; // 左
                    }
                } else {
                    // Z軸方向に吹き飛ぶ（上下）
                    if (knockbackDirection.z > 0.0f) {
                        knockbackDirection = Vector3{ 0.0f, 0.0f, 1.0f };  // 上
                    } else {
                        knockbackDirection = Vector3{ 0.0f, 0.0f, -1.0f }; // 下
                    }
                }
            }
            
            // 吹き飛び状態に設定
            enemyInfo.isKnockedBack = true;
            enemyInfo.knockbackVelocity = knockbackDirection * knockbackSpeed_;
            enemyInfo.knockbackTimer = 0.0f;

            return;
        }
    }
}

void EnemyManager::ClearAllEnemies() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    // すべての敵オブジェクトを削除
    for (auto& enemy : activeEnemies_) {
        if (enemy.object) {
            ctx->RemoveObject3D(enemy.object);
        }
    }

    // 敵リストをクリア
    activeEnemies_.clear();
}

bool EnemyManager::IsWallAt(int x, int z) const {
    if (!walls_) return false;
    
    // 範囲外チェック
    if (x < 0 || x >= wallsWidth_ || z < 0 || z >= wallsHeight_) {
        return false;
    }
    
    // 2次元配列として壁情報にアクセス
    const WallInfo& wall = walls_[z * wallsWidth_ + x];
    return wall.isActive;
}

void EnemyManager::DamageWallAt(int x, int z) {
    if (!walls_) return;
    
    // 範囲外チェック
    if (x < 0 || x >= wallsWidth_ || z < 0 || z >= wallsHeight_) {
        return;
    }
    
    // 2次元配列として壁情報にアクセス
    WallInfo& wall = walls_[z * wallsWidth_ + x];
    
    if (wall.isActive && wall.hp > 0) {
        wall.hp--;
    }
}

void EnemyManager::DestroyWallAt(int x, int z) {
    if (!walls_) return;
    
    // 範囲外チェック
    if (x < 0 || x >= wallsWidth_ || z < 0 || z >= wallsHeight_) {
        return;
    }
    
    // 2次元配列として壁情報にアクセス
    WallInfo& wall = walls_[z * wallsWidth_ + x];
    
    // 壁を即座に破壊
    if (wall.isActive || wall.isMoving) {
        wall.isActive = false;
        wall.isMoving = false;
        wall.hp = 0;
        wall.moveTimer.Reset();
    }
}

bool EnemyManager::IsCrossAreaFromCenter(int x, int z) const {
    // 中心から上下の十字ライン: X座標が4または5
    bool isVerticalLine = (x == 4 || x == 5);
    
    // 中心から左右の十字ライン: Z座標が4または5
    bool isHorizontalLine = (z == 4 || z == 5);
    
    // 中心エリア自体は除外
    bool isCenterArea = (x >= 4 && x <= 5 && z >= 4 && z <= 5);
    
    // 十字ライン上にあり、かつ中心エリアでない場合はtrue
    return (isVerticalLine || isHorizontalLine) && !isCenterArea;
}

} // namespace KashipanEngine