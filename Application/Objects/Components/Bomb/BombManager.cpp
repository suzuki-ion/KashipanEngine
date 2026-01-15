#include "Objects/Components/Bomb/BombManager.h"
#include "Objects/Components/Bomb/ExplosionManager.h"
#include "Objects/Components/Player/PlayerMove.h"

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
    const float dt = GetDeltaTime();

    // ビートの切り替わりを検知（進行度が1.0を超えて0に戻ったとき）
    bool beatOccurred = (prevBpmProgress_ > 0.8f && bpmProgress_ < 0.2f);
    prevBpmProgress_ = bpmProgress_;

    // 爆弾の状態を更新し、寿命切れの爆弾を削除
    auto* ctx = GetOwnerContext();
    for (size_t i = 0; i < activeBombs_.size();) {
        activeBombs_[i].elapsedTime += dt;
        
        // ビートが発生したらカウントを増加
        if (beatOccurred) {
            activeBombs_[i].elapsedBeats++;
        }
        
        // 設定した拍数経過したら削除
        if (activeBombs_[i].elapsedBeats >= bombLifetimeBeats_) {
            // 起爆位置を保存
            Vector3 explosionPosition = activeBombs_[i].position;
            
            // 爆弾を削除
            if (ctx && activeBombs_[i].object) {
                ctx->RemoveObject3D(activeBombs_[i].object);
            }
            activeBombs_.erase(activeBombs_.begin() + static_cast<std::ptrdiff_t>(i));
            
            // 爆発を生成
            if (explosionManager_) {
                explosionManager_->SpawnExplosion(explosionPosition);
            }
        } else {
            ++i;
        }
    }

    // 入力チェック（タイミングが良ければ爆弾を生成）
    if (!input_ || !player_) return;

    const auto& keyboard = input_->GetKeyboard();
    
    // BPM進行度が許容範囲内かチェック
    bool isGoodTiming = (bpmProgress_ <= bpmToleranceRange_) || 
                        (bpmProgress_ >= 1.0f - bpmToleranceRange_);

    // Zキーがトリガーされた場合
    if (keyboard.IsTrigger(Key::Z) && isGoodTiming) {
        // 最大数以下なら爆弾を生成
        if (static_cast<int>(activeBombs_.size()) < maxBombs_) {
            SpawnBomb();
        }
    }
}

void BombManager::SpawnBomb() {
    auto* ctx = GetOwnerContext();
    if (!ctx || !player_) return;

    // プレイヤーの位置と向きを取得
    auto* playerTransform = player_->GetComponent3D<Transform3D>();
    if (!playerTransform) return;

    Vector3 playerPos = playerTransform->GetTranslate();
    
    // PlayerMoveコンポーネントから向きを取得
    auto* playerMove = player_->GetComponent3D<PlayerMove>();
    PlayerDirection direction = PlayerDirection::Down; // デフォルト
    if (playerMove) {
        direction = playerMove->GetPlayerDirection();
    }

    // 爆弾の生成位置を計算
    Vector3 spawnOffset = GetDirectionOffset(direction);
    Vector3 spawnPos = playerPos + spawnOffset * spawnDistance_;
    spawnPos.y = 1.0f; // 地面の少し上に設置

    // マップ外チェック
    if (!IsInsideMap(spawnPos)) {
        return; // マップ外には置けない
    }

    // 既存の爆弾位置チェック
    if (HasBombAtPosition(spawnPos)) {
        return; // 既に爆弾がある場所には置けない
    }

    // 爆弾オブジェクトを生成
    auto modelData = ModelManager::GetModelDataFromFileName("bomb.obj");
    auto bomb = std::make_unique<Model>(modelData);
    bomb->SetName("Bomb_" + std::to_string(activeBombs_.size()));

    if (auto* tr = bomb->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(spawnPos);
        tr->SetScale(Vector3(bombScale_, bombScale_, bombScale_));
    }

    if (auto* mat = bomb->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(0.2f, 0.2f, 0.2f, 1.0f)); // 暗い色で爆弾らしく
    }

    if (collider_ && collider_->GetCollider()) {
        ColliderInfo3D info;
        Math::AABB aabb;
        aabb.min = Vector3{ -0.75f, -0.75f, -0.75f };
        aabb.max = Vector3{ +0.75f, +0.75f, +0.75f };
        info.shape = aabb;
        player_->RegisterComponent<Collision3D>(collider_->GetCollider(), info);
    }

    // レンダラーにアタッチ
    if (screenBuffer_) {
        bomb->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        bomb->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // 爆弾情報を登録
    BombInfo info;
    info.object = bomb.get();
    info.elapsedTime = 0.0f;
    info.elapsedBeats = 0;
    info.beatAccumulator = 0.0f;
    info.position = spawnPos;
    activeBombs_.push_back(info);

    // シーンに追加
    ctx->AddObject3D(std::move(bomb));

    // 効果音再生（オプション）
    auto soundHandle = AudioManager::GetSoundHandleFromFileName("bombPlace.mp3");
    if (soundHandle != AudioManager::kInvalidSoundHandle) {
        AudioManager::Play(soundHandle, 0.5f, 0.0f, false);
    }
}

Vector3 BombManager::GetDirectionOffset(PlayerDirection direction) const {
    switch (direction) {
    case PlayerDirection::Up:
        return Vector3(0.0f, 0.0f, 1.0f);
    case PlayerDirection::Down:
        return Vector3(0.0f, 0.0f, -1.0f);
    case PlayerDirection::Left:
        return Vector3(-1.0f, 0.0f, 0.0f);
    case PlayerDirection::Right:
        return Vector3(1.0f, 0.0f, 0.0f);
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
