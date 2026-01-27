#include "Objects/Components/Bomb/BombManager.h"
#include "Objects/Components/Bomb/ExplosionManager.h"
#include "Objects/Components/Player/PlayerMove.h"
#include "Objects/Components/BPMScaling.h"
#include "Objects/Components/Bomb/BombScaling.h"

namespace KashipanEngine {

BombManager::BombManager(int maxBombs)
    : ISceneComponent("BombManager", 1)
    , maxBombs_(maxBombs) {
}

void BombManager::Initialize() {
    ISceneComponent::Initialize();
    activeBombs_.clear();
    prevBpmProgress_ = 0.0f;
}

void BombManager::Update() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    const float dt = GetDeltaTime();

    // 爆弾のビート更新と寿命管理
    for (auto& bomb : activeBombs_) {
        if (!bomb.object) continue;

        bomb.elapsedTime += dt;
        bomb.beatAccumulator += dt;

        // 1拍分の時間が経過したらビートをカウント
        if (bomb.beatAccumulator >= beatDuration_) {
            bomb.elapsedBeats++;
            bomb.beatAccumulator -= beatDuration_;
        }

        // BPMScalingコンポーネントにBPM進行度を設定
        if (auto* bpmScaling = bomb.object->GetComponent3D<BombScaling>()) {
            bpmScaling->SetBPMProgress(bpmProgress_);
			bpmScaling->SetElapsedBeats(bomb.elapsedBeats);
            bpmScaling->SetMaxBeats(bombLifetimeBeats_);
			bpmScaling->SetBPMDuration(beatDuration_);
            bpmScaling->SetNormalScaleRange(minScale_, maxScale_);
			bpmScaling->SetSpeedScaleRange(minSpeedScale_, maxSpeedScale_);
			bpmScaling->SetDetonationScale(detonationScale_);

            if (auto* mt = bomb.object->GetComponent3D<Material3D>()) {
                if (bpmScaling->IsSpeedScaling()) {
                    mt->SetColor(Vector4::Lerp({ 0.0f,0.0f,0.0f,1.0f }, { 1.0f,0.0f,0.0f,1.0f }, bpmScaling->GetSpeedProgress()));
                } else if(bpmScaling->IsSpeed2Scaling()) {
                    mt->SetColor(Vector4::Lerp({ 0.0f,0.0f,0.0f,1.0f }, { 1.0f,0.0f,0.0f,1.0f }, bpmScaling->GetSpeed2Progress()));
                } else if (bpmScaling->IsSpeed3Scaling()) {
                    mt->SetColor(Vector4{ 1.0f,0.0f,0.0f,1.0f });
                } else {
                    mt->SetColor(Vector4{ 0.0f,0.0f,0.0f,1.0f });
                }
            }
        }
    }

    // 寿命切れまたは起爆フラグが立った爆弾を起爆
    activeBombs_.erase(
        std::remove_if(activeBombs_.begin(), activeBombs_.end(),
            [this, ctx](BombInfo& bomb) {
                const bool expired = bomb.elapsedBeats >= bombLifetimeBeats_;
                const bool shouldDetonate = bomb.shouldDetonate;

                if (expired || shouldDetonate) {
                    // 爆発を生成
                    if (explosionManager_) {
                        explosionManager_->SpawnExplosion(bomb.position);
                    }

                    // オブジェクトを削除
                    if (bomb.object) {
                        ctx->RemoveObject3D(bomb.object);
                    }

                    return true;
                }
                return false;
            }),
        activeBombs_.end()
    );

    // スペースキーで爆弾設置（BPMに合わせて）
    if (inputCommand_) {

        if (inputCommand_->Evaluate("ModeChange").Triggered()) {
            if (useToleranceRange_) {
                useToleranceRange_ = false;
            } else {
                useToleranceRange_ = true;
            }
        }

        if (inputCommand_->Evaluate("Bomb").Triggered()) {
            if (useToleranceRange_) {
                // BPM進行度が許容範囲内かチェック
                if (bpmProgress_ <= 0.0f + bpmToleranceRange_ || bpmProgress_ >= 1.0f - bpmToleranceRange_) {
                    SpawnBomb();
                }
            } else {
                SpawnBomb();
            }
        }
    }

    prevBpmProgress_ = bpmProgress_;
}

void BombManager::SpawnBomb() {
    auto* ctx = GetOwnerContext();
    if (!ctx || !player_ || !screenBuffer_) {
        return;
    }

    // 最大数チェック
    if (static_cast<int>(activeBombs_.size()) >= maxBombs_) {
        return;
    }

    // プレイヤーの位置と向きを取得
    auto* playerMove = player_->GetComponent3D<PlayerMove>();
    if (!playerMove) return;

    auto* playerTransform = player_->GetComponent3D<Transform3D>();
    if (!playerTransform) return;

    const Vector3 playerPos = playerTransform->GetTranslate();
    const PlayerDirection direction = playerMove->GetPlayerDirection();
    const Vector3 offset = GetDirectionOffset(direction);
    const Vector3 bombPos = playerPos + offset;

    // マップ範囲チェック
    if (!IsInsideMap(bombPos)) {
        return;
    }

    // 既に爆弾がある位置には設置できない
    if (HasBombAtPosition(bombPos)) {
        return;
    }

    // 爆弾オブジェクトを生成
    auto modelData = ModelManager::GetModelDataFromFileName("bomb.obj");
    auto bomb = std::make_unique<Model>(modelData);
    bomb->SetName("Bomb_" + std::to_string(activeBombs_.size()));

    if (auto* tr = bomb->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(bombPos);
        tr->SetScale(Vector3(bombScale_));
    }

    if (auto* mat = bomb->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(0.2f, 0.2f, 0.2f, 1.0f));
        auto tex = TextureManager::GetTextureFromFileName("white1x1.png");
        mat->SetTexture(tex);
    }

    // 爆弾オブジェクトのポインタを保存（move前）
    Object3DBase* bombPtr = bomb.get();

    // 衝突判定を追加
    if (collider_ && collider_->GetCollider()) {
        ColliderInfo3D info;
        Math::AABB aabb;
        aabb.min = Vector3{ -0.75f, -0.75f, -0.75f };
        aabb.max = Vector3{ +0.75f, +0.75f, +0.75f };
        info.shape = aabb;
        info.attribute.set(3);  // Bomb属性

        // ラムダで爆弾オブジェクトポインタをキャプチャ
        info.onCollisionEnter = [this, bombPtr](const HitInfo3D& hitInfo) {
            // 衝突相手のオブジェクトからCollision3Dコンポーネントを取得
            if (!hitInfo.otherObject) return;

            auto* otherCollision = hitInfo.otherObject->GetComponent3D<Collision3D>();
            if (!otherCollision) return;

            // Enemyと衝突したかチェック（Enemy属性は1）
            if (otherCollision->GetColliderInfo().attribute.test(1)) {
                // activeBombs_から該当する爆弾を検索してshouldDetonate=trueに設定
                for (auto& b : activeBombs_) {
                    if (b.object == bombPtr) {
                        b.shouldDetonate = true;
                        break;
                    }
                }
            }
        };

        bomb->RegisterComponent<Collision3D>(collider_->GetCollider(), info);
    }

    bomb->RegisterComponent<BombScaling>(Vector3(0.9f, 0.9f, 0.9f), Vector3(1.1f, 1.1f, 1.1f));

    // レンダラーにアタッチ
    if (screenBuffer_) {
        bomb->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        bomb->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // 爆弾情報を登録
    BombInfo info;
    info.object = bombPtr;
    info.elapsedTime = 0.0f;
    info.elapsedBeats = 0;
    info.beatAccumulator = 0.0f;
    info.position = bombPos;
    info.shouldDetonate = false;
    activeBombs_.push_back(info);

    // シーンに追加
    ctx->AddObject3D(std::move(bomb));

    // 設置音再生（オプション）
    auto soundHandle = AudioManager::GetSoundHandleFromFileName("bomb_place.mp3");
    if (soundHandle != AudioManager::kInvalidSoundHandle) {
        AudioManager::Play(soundHandle, 0.1f, 0.0f, false);
    }
}

Vector3 BombManager::GetDirectionOffset(PlayerDirection direction) const {
    switch (direction) {
    case PlayerDirection::Up:
        return Vector3(0.0f, 0.0f, 2.0f);
    case PlayerDirection::Down:
        return Vector3(0.0f, 0.0f, -2.0f);
    case PlayerDirection::Left:
        return Vector3(-2.0f, 0.0f, 0.0f);
    case PlayerDirection::Right:
        return Vector3(2.0f, 0.0f, 0.0f);
    default:
        return Vector3(0.0f, 0.0f, -1.0f);
    }
}

bool BombManager::IsInsideMap(const Vector3& position) const {
    // マップはグリッド単位で管理され、spawnDistance_（通常2.0f）間隔で配置
    // X座標: 0 ～ (mapWidth_ - 1) * spawnDistance_
    // Z座標: 0 ～ (mapHeight_ - 1) * spawnDistance_
    const float maxX = static_cast<float>(mapWidth_ - 1) * spawnDistance_;
    const float maxZ = static_cast<float>(mapHeight_ - 1) * spawnDistance_;

    return position.x >= 0.0f && position.x <= maxX &&
           position.z >= 0.0f && position.z <= maxZ;
}

bool BombManager::HasBombAtPosition(const Vector3& position) const {
    // 位置の許容誤差（グリッドの半分程度）
    const float tolerance = spawnDistance_ * 0.5f;

    for (const auto& bomb : activeBombs_) {
        float dx = std::abs(bomb.position.x - position.x);
        float dz = std::abs(bomb.position.z - position.z);
        
        // XZ平面で近い位置にあるかチェック（Y座標は無視）
        if (dx < tolerance && dz < tolerance) {
            return true;
        }
    }
    return false;
}

void BombManager::DetonateBombsInExplosionRange(const Vector3& explosionCenter, float explosionRange) {
    auto* ctx = GetOwnerContext();
    if (!ctx || !explosionManager_) return;

    // 起爆するボムを収集（イテレート中の削除を避けるため）
    std::vector<size_t> indicesToDetonate;

    for (size_t i = 0; i < activeBombs_.size(); ++i) {
        const Vector3& bombPos = activeBombs_[i].position;
        
        // 十字型の爆発範囲内かチェック
        // X軸方向（explosionCenter.z が同じ位置）
        bool inHorizontalRange = 
            std::abs(bombPos.z - explosionCenter.z) < 1.0f && 
            std::abs(bombPos.x - explosionCenter.x) <= explosionRange;
        
        // Z軸方向（explosionCenter.x が同じ位置）
        bool inVerticalRange = 
            std::abs(bombPos.x - explosionCenter.x) < 1.0f && 
            std::abs(bombPos.z - explosionCenter.z) <= explosionRange;
        
        if (inHorizontalRange || inVerticalRange) {
            indicesToDetonate.push_back(i);
        }
    }

    // 収集したボムを後ろから起爆（インデックスのずれを防ぐ）
    for (auto it = indicesToDetonate.rbegin(); it != indicesToDetonate.rend(); ++it) {
        size_t index = *it;
        
        // 爆発を生成
        if (explosionManager_) {
            explosionManager_->SpawnExplosion(activeBombs_[index].position);
        }
        
        // ボムを削除
        if (activeBombs_[index].object) {
            ctx->RemoveObject3D(activeBombs_[index].object);
        }
        activeBombs_.erase(activeBombs_.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void BombManager::OnEnemyHit(Object3DBase* hitObject) {
    if (!hitObject) return;

    // activeBombs_から該当する爆弾を検索してshouldDetonate=trueに設定
    for (auto& b : activeBombs_) {
        if (b.object == hitObject) {
            b.shouldDetonate = true;
            break;
        }
    }
}

#if defined(USE_IMGUI)
void BombManager::ShowImGui() {
    ImGui::Text("BombManager");
    ImGui::Separator();
    ImGui::Text("Active Bombs: %d / %d", static_cast<int>(activeBombs_.size()), maxBombs_);
    ImGui::DragInt("Max Bombs", &maxBombs_, 1, 1, 20);
    ImGui::DragFloat("Spawn Distance", &spawnDistance_, 0.1f, 0.5f, 10.0f);
    ImGui::DragFloat("Bomb Scale", &bombScale_, 0.1f, 0.1f, 3.0f);
    ImGui::DragInt("Bomb Lifetime (beats)", &bombLifetimeBeats_, 1, 1, 16);
    ImGui::DragFloat("BPM Tolerance", &bpmToleranceRange_, 0.01f, 0.0f, 0.5f);
    ImGui::Text("Current BPM Progress: %.2f", bpmProgress_);
    ImGui::Text("Map Size: %d x %d", mapWidth_, mapHeight_);
    
    if (ImGui::TreeNode("Active Bombs Details")) {
        for (size_t i = 0; i < activeBombs_.size(); ++i) {
            ImGui::Text("Bomb %zu: Beats=%d/%d, Pos=(%.1f, %.1f, %.1f)", 
                i, activeBombs_[i].elapsedBeats, bombLifetimeBeats_,
                activeBombs_[i].position.x, activeBombs_[i].position.y, activeBombs_[i].position.z);
        }
        ImGui::TreePop();
    }
}
#endif

} // namespace KashipanEngine
