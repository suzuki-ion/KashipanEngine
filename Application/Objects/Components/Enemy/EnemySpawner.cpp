#include "EnemySpawner.h"
#include <random>

namespace KashipanEngine {

void EnemySpawner::Initialize() {
    ISceneComponent::Initialize();
    lastSpawnBeat_ = -spawnInterval_; // 初回スポーンがすぐに行われるように
}

void EnemySpawner::Update() {
    if (!enemyManager_ || spawnPoints_.empty()) return;

    if (!bpmSystem_) return;

    const int currentBeat = bpmSystem_->GetCurrentBeat();

    // スポーン間隔が経過したかチェック
    if (currentBeat - lastSpawnBeat_ >= spawnInterval_) {
        lastSpawnBeat_ = currentBeat;
        SpawnEnemy();
    }
}

void EnemySpawner::AddSpawnPoint(const Vector3& position) {
    spawnPoints_.push_back({ position });
}

void EnemySpawner::SpawnEnemy() {
    if (spawnPoints_.empty()) return;

    EnemyType type = DetermineEnemyType();
    const SpawnPoint& spawnPoint = SelectSpawnPoint();
    EnemyDirection dir = ChooseSpawnDirection_NoOutward(spawnPoint.position);

    enemyManager_->SpawnEnemy(type, dir, spawnPoint.position);
}

EnemyType EnemySpawner::DetermineEnemyType() const {
    // 現在はBasicのみ。将来的にランダムや条件分岐を追加可能
    return EnemyType::Basic;
}

const SpawnPoint& EnemySpawner::SelectSpawnPoint() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dis(0, spawnPoints_.size() - 1);
    
    return spawnPoints_[dis(gen)];
}

EnemyDirection EnemySpawner::ChooseSpawnDirection_NoOutward(
    const Vector3& pos,
    int mapW, int mapH,
    float tile) const {
    const float minX = 0.0f;
    const float minZ = 0.0f;
    const float maxX = (mapW - 1) * tile;
    const float maxZ = (mapH - 1) * tile;

    auto isNear = [](float a, float b) { return std::abs(a - b) < 0.001f; };

    bool onLeft = isNear(pos.x, minX);
    bool onRight = isNear(pos.x, maxX);
    bool onTop = isNear(pos.z, minZ);
    bool onBottom = isNear(pos.z, maxZ);

    std::vector<EnemyDirection> candidates = {
        EnemyDirection::Up,
        EnemyDirection::Down,
        EnemyDirection::Left,
        EnemyDirection::Right,
    };

    // 外へ向く方向を消す
    auto eraseDir = [&](EnemyDirection d) {
        candidates.erase(std::remove(candidates.begin(), candidates.end(), d), candidates.end());
        };

    if (onLeft)   eraseDir(EnemyDirection::Left);
    if (onRight)  eraseDir(EnemyDirection::Right);
    if (onTop)    eraseDir(EnemyDirection::Up);
    if (onBottom) eraseDir(EnemyDirection::Down);

    // 念のため（理屈上は空にならないが、座標ズレ等で全部消えた場合の保険）
    if (candidates.empty()) {
        // とりあえず内側優先：中心方向に近いものを選ぶ、などにしても良い
        return EnemyDirection::Down;
    }

    // ランダムに1つ
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
    return candidates[dist(rng)];
}

} // namespace KashipanEngine