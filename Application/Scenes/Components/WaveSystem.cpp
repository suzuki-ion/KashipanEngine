#include "WaveSystem.h"
#include "Objects/Components/Enemy/EnemySpawnParticle.h"

namespace KashipanEngine {

void WaveSystem::Initialize() {
    // 初期化
    ResetSystem();
}

void WaveSystem::Update() {
    if (!isSystemRunning_ || !bpmSystem_) return;

    int currentBeat = bpmSystem_->GetCurrentBeat();

    // プール内の全パーティクルにconfigを適用（EnemySpawnerと同じ）
    for (auto* particle : particlePool_) {
        if (auto* spawnParticle = particle->GetComponent3D<EnemySpawnParticle>()) {
            spawnParticle->SetConfig(spawnParticleConfig_);
        }
    }

    // パーティクル放出中の位置に対して毎フレーム各位置から1粒ずつパーティクルを発生
    for (const auto& emitPos : activeEmitPositions_) {
        SpawnParticlesAtPosition(emitPos, 1); // 各位置から1粒ずつ
    }

    // 新しい拍が発生した場合のみ処理
    if (currentBeat == lastProcessedBeat_) return;
    lastProcessedBeat_ = currentBeat;

    globalBeatCount_++;

    // Wave開始待機中
    if (isWaitingForWaveStart_) {
        delayBeatCount_++;
        if (delayBeatCount_ >= preWaveDelayBeats_) {
            // Wave開始
            isWaitingForWaveStart_ = false;
            isWaveStarted_ = true;
            waveBeatCount_ = 0;
            delayBeatCount_ = 0;

            // 現在のWaveのスポーンデータを予約リストに追加
            if (currentWaveIndex_ < static_cast<int>(waveDataList_.size())) {
                const auto& waveData = waveDataList_[currentWaveIndex_];
                for (const auto& spawnData : waveData.spawnList) {
                    ScheduledSpawn scheduled{};
                    scheduled.targetBeat = spawnData.spawnBeat;
                    scheduled.mapX = spawnData.mapX;
                    scheduled.mapZ = spawnData.mapZ;
                    scheduled.enemyType = spawnData.enemyType;
                    scheduled.particleSpawned = false;
                    scheduledSpawns_.push_back(scheduled);
                }
            }
        }
        return;
    }

    // 次のWave待機中
    if (isWaitingForNextWave_) {
        delayBeatCount_++;
        if (delayBeatCount_ >= postWaveDelayBeats_) {
            TransitionToNextWave();
        }
        return;
    }

    // Wave進行中
    if (isWaveStarted_) {
        waveBeatCount_++;

        // パーティクル予約処理（新しい拍で発生タイミングをチェック）
        ProcessScheduledParticles();

        // スポーン予約処理
        ProcessScheduledSpawns();

        // Wave終了チェック
        if (currentWaveIndex_ < static_cast<int>(waveDataList_.size())) {
            const auto& waveData = waveDataList_[currentWaveIndex_];
            if (waveBeatCount_ >= waveData.duration) {
                // Wave終了
                isWaveStarted_ = false;
                isWaitingForNextWave_ = true;
                delayBeatCount_ = 0;
                activeEmitPositions_.clear(); // パーティクル放出停止
            }
        }
    }
}

void WaveSystem::AddWaveData(const WaveData& waveData) {
    waveDataList_.push_back(waveData);
}

void WaveSystem::ClearWaveData() {
    waveDataList_.clear();
}

void WaveSystem::ResetSystem() {
    globalBeatCount_ = 0;
    waveBeatCount_ = 0;
    delayBeatCount_ = 0;
    lastProcessedBeat_ = -1;

    currentWave_ = Wave::Wave1;
    currentWaveIndex_ = 0;

    isSystemRunning_ = false;
    isWaveStarted_ = false;
    isWaitingForWaveStart_ = true;
    isWaitingForNextWave_ = false;
    isAllWavesCompleted_ = false;

    scheduledSpawns_.clear();
    activeEmitPositions_.clear();
}

void WaveSystem::StartSystem() {
    if (isSystemRunning_) return;

    ResetSystem();
    isSystemRunning_ = true;
    isWaitingForWaveStart_ = true;
}

void WaveSystem::StopSystem() {
    isSystemRunning_ = false;
    scheduledSpawns_.clear();
    activeEmitPositions_.clear();
}

void WaveSystem::ScheduleEnemySpawn(int beatInWave, int mapX, int mapZ, EnemyType enemyType) {
    // 現在のWaveデータに敵スポーン情報を追加
    if (currentWaveIndex_ < static_cast<int>(waveDataList_.size())) {
        EnemySpawnData spawnData{};
        spawnData.spawnBeat = beatInWave;
        spawnData.mapX = mapX;
        spawnData.mapZ = mapZ;
        spawnData.enemyType = enemyType;
        waveDataList_[currentWaveIndex_].spawnList.push_back(spawnData);
    }
}

void WaveSystem::InitializeParticlePool(int particlesPerFrame) {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    particlesPerFrame_ = particlesPerFrame;
    particlePool_.reserve(kParticlePoolSize_);

    for (int i = 0; i < kParticlePoolSize_; ++i) {
        auto particle = std::make_unique<Box>();
        particle->SetName("WaveSpawnParticle_" + std::to_string(i));

        if (auto* tr = particle->GetComponent3D<Transform3D>()) {
            tr->SetScale(Vector3{ 0.0f, 0.0f, 0.0f });
        }

        if (auto* mat = particle->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4{ 0.8f, 0.8f, 0.8f, 1.0f });
            mat->SetEnableLighting(true);
        }

        // デフォルトのパーティクル設定
        ParticleConfig defaultConfig{};
        defaultConfig.initialSpeed = 5.0f;
        defaultConfig.speedVariation = 1.5f;
        defaultConfig.lifeTimeSec = 1.0f;
        defaultConfig.gravity = -5.0f;
        defaultConfig.damping = 0.98f;
        defaultConfig.spreadAngle = 0.0f;
        defaultConfig.baseScale = Vector3{ 0.25f, 0.25f, 0.25f };

        particle->RegisterComponent<EnemySpawnParticle>(defaultConfig);

        if (screenBuffer_) {
            particle->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }

        particlePool_.push_back(particle.get());
        ctx->AddObject3D(std::move(particle));
    }
}

void WaveSystem::TransitionToNextWave() {
    currentWaveIndex_++;

    if (currentWaveIndex_ >= static_cast<int>(waveDataList_.size())) {
        // 全Wave終了
        isAllWavesCompleted_ = true;
        isSystemRunning_ = false;
        isWaitingForNextWave_ = false;

        if (onAllWavesCompletedCallback_) {
            onAllWavesCompletedCallback_();
        }
        return;
    }

    // 次のWaveへ
    currentWave_ = static_cast<Wave>(currentWaveIndex_);
    isWaitingForNextWave_ = false;
    isWaitingForWaveStart_ = true;
    delayBeatCount_ = 0;
    scheduledSpawns_.clear();
}

void WaveSystem::SpawnEnemyInternal(int mapX, int mapZ, EnemyType enemyType) {
    if (!enemyManager_) return;

    Vector3 position = MapToWorldPosition(mapX, mapZ);
    EnemyDirection direction = DetermineDirection(mapX, mapZ);

    enemyManager_->SpawnEnemy(enemyType, direction, position);
}

void WaveSystem::SpawnParticlesAtPosition(const Vector3& position, int count) {
    int particlesSpawned = 0;

    for (auto* particle : particlePool_) {
        if (particlesSpawned >= count) break;

        auto* spawnParticle = particle->GetComponent3D<EnemySpawnParticle>();
        if (!spawnParticle) continue;

        if (!spawnParticle->IsAlive()) {
            spawnParticle->SetConfig(spawnParticleConfig_);
            spawnParticle->Spawn(position);
            particlesSpawned++;
        }
    }
}

Vector3 WaveSystem::MapToWorldPosition(int mapX, int mapZ) const {
    return Vector3{
        static_cast<float>(mapX) * tileSize_,
        0.0f,
        static_cast<float>(mapZ) * tileSize_
    };
}

EnemyDirection WaveSystem::DetermineDirection(int mapX, int mapZ) const {
    // マップの端から中心に向かう方向を決定
    if (mapZ == mapHeight_ - 1) {
        return EnemyDirection::Down;
    } else if (mapZ == 0) {
        return EnemyDirection::Up;
    } else if (mapX == 0) {
        return EnemyDirection::Right;
    } else if (mapX == mapWidth_ - 1) {
        return EnemyDirection::Left;
    }

    // デフォルト
    return EnemyDirection::Down;
}

void WaveSystem::ProcessScheduledParticles() {
    // 同じ拍で複数の敵生成予告がある場合、全ての位置を収集

    for (auto& spawn : scheduledSpawns_) {
        // パーティクルを先行して発生させる
        int particleSpawnBeat = spawn.targetBeat - spawnParticleLeadBeats_;
        
        // パーティクル発生開始タイミング
        if (waveBeatCount_ == particleSpawnBeat && !spawn.particleSpawned) {
            Vector3 position = MapToWorldPosition(spawn.mapX, spawn.mapZ);
            activeEmitPositions_.push_back(position);
            spawn.particleSpawned = true;
        }
    }

    // 敵生成タイミングに達したら、その位置のパーティクル放出を停止
    for (const auto& spawn : scheduledSpawns_) {
        if (waveBeatCount_ >= spawn.targetBeat) {
            Vector3 position = MapToWorldPosition(spawn.mapX, spawn.mapZ);
            // activeEmitPositions_から削除
            auto it = std::find_if(activeEmitPositions_.begin(), activeEmitPositions_.end(),
                [&position](const Vector3& pos) {
                    return std::abs(pos.x - position.x) < 0.01f && 
                           std::abs(pos.z - position.z) < 0.01f;
                });
            if (it != activeEmitPositions_.end()) {
                activeEmitPositions_.erase(it);
            }
        }
    }
}

void WaveSystem::ProcessScheduledSpawns() {
    // 処理済みスポーンを削除するためのリスト
    std::vector<size_t> toRemove;

    for (size_t i = 0; i < scheduledSpawns_.size(); ++i) {
        const auto& spawn = scheduledSpawns_[i];
        if (waveBeatCount_ >= spawn.targetBeat) {
            SpawnEnemyInternal(spawn.mapX, spawn.mapZ, spawn.enemyType);
            toRemove.push_back(i);
        }
    }

    // 後ろから削除（インデックスがずれないように）
    for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
        scheduledSpawns_.erase(scheduledSpawns_.begin() + *it);
    }
}

#if defined(USE_IMGUI)
void WaveSystem::ShowImGui() {
    if (ImGui::TreeNode("WaveSystem")) {
        ImGui::Text("System Running: %s", isSystemRunning_ ? "Yes" : "No");
        ImGui::Text("Current Wave: %d", currentWaveIndex_ + 1);
        ImGui::Text("Wave Started: %s", isWaveStarted_ ? "Yes" : "No");
        ImGui::Text("Global Beat: %d", globalBeatCount_);
        ImGui::Text("Wave Beat: %d", waveBeatCount_);
        ImGui::Text("Delay Beat: %d", delayBeatCount_);
        ImGui::Text("All Waves Completed: %s", isAllWavesCompleted_ ? "Yes" : "No");
        ImGui::Text("Scheduled Spawns: %zu", scheduledSpawns_.size());
        ImGui::Text("Active Emit Positions: %zu", activeEmitPositions_.size());

        ImGui::Separator();
        ImGui::DragInt("Pre-Wave Delay", &preWaveDelayBeats_, 1, 1, 16);
        ImGui::DragInt("Post-Wave Delay", &postWaveDelayBeats_, 1, 1, 16);
        ImGui::DragInt("Spawn Particle Lead", &spawnParticleLeadBeats_, 1, 1, 8);

        if (ImGui::Button("Start System")) {
            StartSystem();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop System")) {
            StopSystem();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset System")) {
            ResetSystem();
        }

        ImGui::TreePop();
    }
}
#endif

} // namespace KashipanEngine
