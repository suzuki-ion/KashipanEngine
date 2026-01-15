#include "BpmbSpawn.h"
#include "PlayerMove.h"
#include "Scenes/TestScene.h"
#include "../BPMScaling.h"

namespace KashipanEngine {

std::optional<bool> BombSpawn::Update() {
    // 既存のボムにBPM進行度を同期
    for (auto* bomb : bombObjects_) {
        if (!bomb) continue;

        if (auto* bpmScaling = bomb->GetComponent3D<BPMScaling>()) {
            bpmScaling->SetBPMProgress(bpmProgress_);
        }
    }

    // Zキーで爆弾設置
    if (input_ && input_->GetKeyboard().IsTrigger(Key::Z)) {
        TryPlaceBomb();
    }

    if (input_ && input_->GetKeyboard().IsTrigger(Key::X)) {
		ClearAllBombs();
    }

    return std::nullopt;
}

void BombSpawn::TryPlaceBomb() {
    // 設置上限数チェック
    if (GetBombCount() >= maxBombs_) {
        return;
    }

    // BPM進行度のチェック（タイミングが合っているか）
    float distanceFromBeat = std::min(bpmProgress_, 1.0f - bpmProgress_);
    if (distanceFromBeat > bpmToleranceRange_) {
        return; // タイミングが合っていない
    }

    // PlayerMoveコンポーネントから向きを取得
    auto* playerMove = GetOwner3DContext()->GetComponent<PlayerMove>();
    if (!playerMove) {
        return;
    }

    PlayerDirection direction = playerMove->GetPlayerDirection();

    // プレイヤーの現在位置を取得
    auto* transform = GetOwner3DContext()->GetComponent<Transform3D>();
    if (!transform) {
        return;
    }

    Vector3 currentPos = transform->GetTranslate();

    // 爆弾の設置位置を計算
    Vector3 bombPosition = CalculateBombPosition(currentPos, direction);

    // マップ座標に変換してチェック
    int mapX, mapZ;
    WorldToMapCoordinates(bombPosition, mapX, mapZ);

    // マップの外には設置できない
    if (!IsValidMapPosition(mapX, mapZ)) {
        return;
    }

    // 既にボムが存在する位置には設置できない
    if (IsBombAtPosition(mapX, mapZ)) {
        return;
    }

    // 爆弾を作成してシーンに追加
    CreateBomb(bombPosition);
}

Vector3 BombSpawn::CalculateBombPosition(const Vector3& currentPos, PlayerDirection direction) const {
    Vector3 offset(0.0f, 0.0f, 0.0f);

    switch (direction) {
    case PlayerDirection::Up:
        offset = Vector3(0.0f, 0.0f, bombOffset_);
        break;
    case PlayerDirection::Down:
        offset = Vector3(0.0f, 0.0f, -bombOffset_);
        break;
    case PlayerDirection::Left:
        offset = Vector3(-bombOffset_, 0.0f, 0.0f);
        break;
    case PlayerDirection::Right:
        offset = Vector3(bombOffset_, 0.0f, 0.0f);
        break;
    }

    return currentPos + offset;
}

bool BombSpawn::IsValidMapPosition(int mapX, int mapZ) const {
    return mapX >= 0 && mapX < mapWidth_ && mapZ >= 0 && mapZ < mapHeight_;
}

bool BombSpawn::IsBombAtPosition(int mapX, int mapZ) const {
    for (const auto* bomb : bombObjects_) {
        if (!bomb) continue;

        auto* transform = bomb->GetComponent3D<Transform3D>();
        if (!transform) continue;

        Vector3 bombPos = transform->GetTranslate();
        int bombMapX, bombMapZ;
        WorldToMapCoordinates(bombPos, bombMapX, bombMapZ);

        if (bombMapX == mapX && bombMapZ == mapZ) {
            return true;
        }
    }
    return false;
}

void BombSpawn::WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const {
    outMapX = static_cast<int>(worldPos.x / 2.0f);
    outMapZ = static_cast<int>(worldPos.z / 2.0f);
}

void BombSpawn::CreateBomb(const Vector3& position) {
    if (!scene_) return;

    auto modelData = ModelManager::GetModelDataFromFileName("bomb.obj");
    auto bomb = std::make_unique<Model>(modelData);
    bomb->SetName("Bomb_" + std::to_string(bombObjects_.size()));

    if (auto* tr = bomb->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(position);
        tr->SetScale(Vector3(bombScale_, bombScale_, bombScale_));
    }

    if (auto* mat = bomb->GetComponent3D<Material3D>()) {
        mat->SetEnableLighting(true);
        mat->SetColor(Vector4(1.0f, 0.0f, 0.0f, 1.0f)); // 赤色
    }

    // BPMスケーリングコンポーネントを追加
    bomb->RegisterComponent<BPMScaling>(0.8f, 1.0f);
    if (auto* bpmScaling = bomb->GetComponent3D<BPMScaling>()) {
        bpmScaling->SetBPMProgress(bpmProgress_);
    }

    if (screenBuffer_) {
        bomb->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
    }
    if (shadowMapBuffer_) {
        bomb->AttachToRenderer(shadowMapBuffer_, "Object3D.ShadowMap.DepthOnly");
    }

    // 爆弾リストに追加
    Object3DBase* bombPtr = bomb.get();
    bombObjects_.push_back(bombPtr);

    // TestSceneにキャストしてAddObject3Dを呼び出し
    if (auto* testScene = dynamic_cast<TestScene*>(scene_)) {
        testScene->AddBombObject(std::move(bomb));
    }
}

void BombSpawn::RemoveBomb(size_t index) {
    if (index < bombObjects_.size()) {
        bombObjects_.erase(bombObjects_.begin() + index);
    }
}

void BombSpawn::ClearAllBombs() {
    bombObjects_.clear();
}

void BombSpawn::ShowImGui() {
#ifdef USE_IMGUI
    if (ImGui::TreeNode("BombSpawn")) {
        ImGui::Text("Current Bombs: %d / %d", GetBombCount(), maxBombs_);
        ImGui::SliderInt("Max Bombs", &maxBombs_, 1, 10);
        ImGui::SliderFloat("Bomb Offset", &bombOffset_, 1.0f, 5.0f);
        ImGui::SliderFloat("Bomb Scale", &bombScale_, 0.5f, 2.0f);
        ImGui::SliderFloat("BPM Tolerance", &bpmToleranceRange_, 0.05f, 0.5f);
        ImGui::Text("BPM Progress: %.2f", bpmProgress_);
        ImGui::TreePop();
    }
#endif
}

} // namespace KashipanEngine
