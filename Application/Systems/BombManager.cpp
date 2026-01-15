#include "BombManager.h"
#include "Objects/Components/BPMScaling.h"

namespace KashipanEngine {

void BombManager::Update(const Input* input) {
    // 全ての爆弾にBPM進行度を同期
    for (auto& bomb : bombs_) {
        if (!bomb) continue;

        if (auto* bpmScaling = bomb->GetComponent3D<BPMScaling>()) {
            bpmScaling->SetBPMProgress(bpmProgress_);
        }
    }

    // 登録されているBombSpawnコンポーネントから入力を処理
    if (input) {
        for (auto* spawner : registeredSpawners_) {
            if (!spawner) continue;

            // Zキーで爆弾設置
            if (input->GetKeyboard().IsTrigger(Key::Z)) {
                auto request = spawner->TryCreatePlacementRequest();
                if (request.has_value()) {
                    if (ProcessPlacementRequest(request.value())) {
                        spawner->OnBombPlaced();
                    }
                }
            }

            // Xキーで全クリア
            if (input->GetKeyboard().IsTrigger(Key::X)) {
                ClearAllBombs();
                spawner->OnAllBombsCleared();
            }
        }
    }
}

bool BombManager::ProcessPlacementRequest(const BombPlacementRequest& request) {
    // 既に爆弾が存在する位置には設置できない
    if (IsBombAtPosition(request.mapX, request.mapZ)) {
        return false;
    }

    // 爆弾オブジェクトを作成
    auto bomb = CreateBombObject(request);
    if (!bomb) {
        return false;
    }

    bombs_.push_back(std::move(bomb));
    return true;
}

bool BombManager::IsBombAtPosition(int mapX, int mapZ) const {
    for (const auto& bomb : bombs_) {
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

void BombManager::RemoveBomb(size_t index) {
    if (index < bombs_.size()) {
        bombs_.erase(bombs_.begin() + index);
        
        // 登録されているSpawnerに通知
        for (auto* spawner : registeredSpawners_) {
            if (spawner) {
                spawner->OnBombRemoved();
            }
        }
    }
}

void BombManager::ClearAllBombs() {
    bombs_.clear();
    
    // 登録されているSpawnerに通知
    for (auto* spawner : registeredSpawners_) {
        if (spawner) {
            spawner->OnAllBombsCleared();
        }
    }
}

std::unique_ptr<Object3DBase> BombManager::TakeBomb(size_t index) {
    if (index >= bombs_.size()) {
        return nullptr;
    }

    auto bomb = std::move(bombs_[index]);
    bombs_.erase(bombs_.begin() + index);
    return bomb;
}

void BombManager::RegisterBombSpawn(BombSpawn* bombSpawn) {
    if (!bombSpawn) return;

    // 既に登録されているかチェック
    for (auto* spawner : registeredSpawners_) {
        if (spawner == bombSpawn) {
            return;
        }
    }

    registeredSpawners_.push_back(bombSpawn);
}

void BombManager::UnregisterBombSpawn(BombSpawn* bombSpawn) {
    registeredSpawners_.erase(
        std::remove(registeredSpawners_.begin(), registeredSpawners_.end(), bombSpawn),
        registeredSpawners_.end()
    );
}

std::unique_ptr<Object3DBase> BombManager::CreateBombObject(const BombPlacementRequest& request) {
    auto modelData = ModelManager::GetModelDataFromFileName("bomb.obj");
    auto bomb = std::make_unique<Model>(modelData);
    bomb->SetName("Bomb_" + std::to_string(bombs_.size()));

    if (auto* tr = bomb->GetComponent3D<Transform3D>()) {
        tr->SetTranslate(request.position);
        tr->SetScale(Vector3(request.scale, request.scale, request.scale));
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

    return bomb;
}

void BombManager::WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const {
    outMapX = static_cast<int>(worldPos.x / 2.0f);
    outMapZ = static_cast<int>(worldPos.z / 2.0f);
}

} // namespace KashipanEngine
