#pragma once
#include <KashipanEngine.h>
#include "EnemyManager.h"
#include "EnemyType.h"
#include <vector>
#include "Scenes/Components/BPM/BPMSystem.h"

namespace KashipanEngine {

struct SpawnPoint {
    Vector3 position;
};

class EnemySpawner final : public ISceneComponent {
public:
    explicit EnemySpawner(int spawnInterval = 4)
        : ISceneComponent("EnemySpawner", 50)
        , spawnInterval_(spawnInterval) {
    }

    ~EnemySpawner() override = default;

    void Initialize() override;
    void Update() override;

    // スポーン地点を追加
    void AddSpawnPoint(const Vector3& position);

    // スポーン間隔を設定（拍数）
    void SetSpawnInterval(int interval) { spawnInterval_ = interval; }

	void SetEnemyManager(EnemyManager* manager) { enemyManager_ = manager; }

	void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }
private:
    void SpawnEnemy();
    EnemyType DetermineEnemyType() const;
    EnemyDirection ChooseSpawnDirection_NoOutward(const Vector3& pos, int mapW = 13, int mapH = 13, float tile = 2.0f) const;
    const SpawnPoint& SelectSpawnPoint() const;

    std::vector<SpawnPoint> spawnPoints_;
    int spawnInterval_ = 4; // 何拍ごとにスポーンするか
    int lastSpawnBeat_ = -1;
    EnemyManager* enemyManager_ = nullptr;
	BPMSystem* bpmSystem_ = nullptr;
};

} // namespace KashipanEngine