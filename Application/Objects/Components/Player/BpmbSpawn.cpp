#include "BpmbSpawn.h"
#include "PlayerMove.h"
#include "Scenes/TestScene.h"
#include "../BPMScaling.h"

namespace KashipanEngine {

std::optional<bool> BombSpawn::Update() {
    // 入力チェックのみを行う
    // 実際の処理はBombManagerが行う
    return std::nullopt;
}

std::optional<BombPlacementRequest> BombSpawn::TryCreatePlacementRequest() {
    // 設置上限数チェック
    if (currentBombCount_ >= maxBombs_) {
        return std::nullopt;
    }

    // BPM進行度のチェック（タイミングが合っているか）
    float distanceFromBeat = std::min(bpmProgress_, 1.0f - bpmProgress_);
    if (distanceFromBeat > bpmToleranceRange_) {
        return std::nullopt; // タイミングが合っていない
    }

    // PlayerMoveコンポーネントから向きを取得
    auto* playerMove = GetOwner3DContext()->GetComponent<PlayerMove>();
    if (!playerMove) {
        return std::nullopt;
    }

    PlayerDirection direction = playerMove->GetPlayerDirection();

    // プレイヤーの現在位置を取得
    auto* transform = GetOwner3DContext()->GetComponent<Transform3D>();
    if (!transform) {
        return std::nullopt;
    }

    Vector3 currentPos = transform->GetTranslate();

    // 爆弾の設置位置を計算
    Vector3 bombPosition = CalculateBombPosition(currentPos, direction);

    // マップ座標に変換してチェック
    int mapX, mapZ;
    WorldToMapCoordinates(bombPosition, mapX, mapZ);

    // マップの外には設置できない
    if (!IsValidMapPosition(mapX, mapZ)) {
        return std::nullopt;
    }

    // リクエストを生成して返す
    BombPlacementRequest request;
    request.position = bombPosition;
    request.mapX = mapX;
    request.mapZ = mapZ;
    request.scale = bombScale_;

    return request;
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

void BombSpawn::WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const {
    outMapX = static_cast<int>(worldPos.x / 2.0f);
    outMapZ = static_cast<int>(worldPos.z / 2.0f);
}

#if defined(USE_IMGUI)
void BombSpawn::ShowImGui() {
    if (ImGui::TreeNode("BombSpawn")) {
        ImGui::Text("Current Bombs: %d / %d", currentBombCount_, maxBombs_);
        ImGui::SliderInt("Max Bombs", &maxBombs_, 1, 10);
        ImGui::SliderFloat("Bomb Offset", &bombOffset_, 1.0f, 5.0f);
        ImGui::SliderFloat("Bomb Scale", &bombScale_, 0.5f, 2.0f);
        ImGui::SliderFloat("BPM Tolerance", &bpmToleranceRange_, 0.05f, 0.5f);
        ImGui::Text("BPM Progress: %.2f", bpmProgress_);
        ImGui::TreePop();
    }
}
#endif

} // namespace KashipanEngine} // namespace KashipanEngine