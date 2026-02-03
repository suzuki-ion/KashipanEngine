#include "WaveSystem.h"
#include "Objects/Components/Enemy/EnemySpawnParticle.h"
#include "Objects/Components/BPMScaling.h"

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

    // カウントダウン表示を更新
    UpdateCountdown();

    // Wave表示を更新
    UpdateWaveDisplay();

    // Wave切り替えアニメーションを更新
    UpdateWaveTransitionAnimation();

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

            // カウントダウンを非表示
            HideCountdown();

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
            
            // duration超過フラグを設定
            if (waveBeatCount_ >= waveData.duration && !isDurationExceeded_) {
                isDurationExceeded_ = true;
                activeEmitPositions_.clear(); // パーティクル放出停止
            }

            // duration超過後、全敵が死亡したかチェック
            if (isDurationExceeded_) {
                // 現在のWaveで生成した敵がすべて死亡しているかチェック
                bool allEnemiesDead = true;
                if (enemyManager_) {
                    for (int enemyID : currentWaveEnemyIDs_) {
                        if (enemyManager_->IsEnemyAlive(enemyID)) {
                            allEnemiesDead = false;
                            break;
                        }
                    }
                }

                // すべての敵が死亡していればWave終了
                if (allEnemiesDead) {
                    // Wave終了 - アニメーション開始
                    isWaveStarted_ = false;
                    isWaitingForNextWave_ = true;
                    delayBeatCount_ = 0;
                    isDurationExceeded_ = false;

                    // Wave切り替えアニメーション開始（次のWaveへの遷移前に開始）
                    int nextWaveIndex = currentWaveIndex_ + 1;
                    if (nextWaveIndex < static_cast<int>(waveDataList_.size())) {
                        nextWaveToDisplay_ = nextWaveIndex + 1;
                        waveTransitionState_ = WaveTransitionState::MovingOut;
                        waveTransitionTimer_ = 0.0f;
                    } else {
                        // 全Wave終了の場合
                        nextWaveToDisplay_ = -1;
                        waveTransitionState_ = WaveTransitionState::MovingOut;
                        waveTransitionTimer_ = 0.0f;
                    }
                }
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
    isDurationExceeded_ = false;

    scheduledSpawns_.clear();
    activeEmitPositions_.clear();

    currentWaveSpawnedEnemyCount_ = 0;
    currentWaveEnemyIDs_.clear();

    // Wave切り替えアニメーションをリセット
    waveTransitionState_ = WaveTransitionState::Idle;
    waveTransitionTimer_ = 0.0f;
    nextWaveToDisplay_ = -1;
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

void WaveSystem::InitializeCountdownModels() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    // 数字モデル 0-9 を初期化
    for (int i = 0; i < kMaxCountdownNumbers_; ++i) {
        std::string modelName = std::to_string(i) + ".obj";
        auto modelData = ModelManager::GetModelDataFromFileName(modelName);
        auto obj = std::make_unique<Model>(modelData);
        
        obj->SetName("CountdownNumber_" + std::to_string(i));
        obj->SetUniqueBatchKey();

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(countdownPosition_);
            tr->SetParentTransform(parentTransform_);
            tr->SetScale(Vector3(0.0f));  // 初期状態は非表示
        }

        if (auto* mt = obj->GetComponent3D<Material3D>()) {
            mt->SetColor(Vector4(0.75f, 0.75f, 0.75f, 0.0f));
            mt->SetEnableLighting(true);
        }

        // BPMScalingコンポーネントを追加してビート連動させる
        obj->RegisterComponent<BPMScaling>(
            Vector3(countdownScale_ * 0.9f), 
            Vector3(countdownScale_ * 1.1f), 
            EaseType::EaseOutExpo
        );

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }

        countdownNumbers_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
}

void WaveSystem::InitializeWaveModels() {
    auto* ctx = GetOwnerContext();
    if (!ctx) return;

    // Wave1～Wave9 モデルを初期化
    for (int i = 0; i < kMaxWaveNumbers_; ++i) {
        std::string modelName = "Wave" + std::to_string(i + 1) + ".obj";
        auto modelData = ModelManager::GetModelDataFromFileName(modelName);
        auto obj = std::make_unique<Model>(modelData);
        
        obj->SetName("WaveDisplay_" + std::to_string(i + 1));
        obj->SetUniqueBatchKey();

        if (auto* tr = obj->GetComponent3D<Transform3D>()) {
            tr->SetTranslate(waveDisplayPosition_);
			tr->SetRotate(Vector3(-0.05f, 0.4f, -0.1f));
            tr->SetParentTransform(parentTransform_);
            tr->SetScale(Vector3(1.0f));  // 初期状態は非表示
        }

        if (auto* mat = obj->GetComponent3D<Material3D>()) {
            mat->SetColor(Vector4(0.75f, 0.75f, 0.75f, 0.0f));  // 初期状態は透明
            mat->SetEnableLighting(true);
        }

        // BPMScalingコンポーネントを追加してビート連動させる
        obj->RegisterComponent<BPMScaling>(
            Vector3(waveDisplayScale_ * 0.9f), 
            Vector3(waveDisplayScale_ * 1.1f), 
            EaseType::EaseOutExpo
        );

        if (screenBuffer_) {
            obj->AttachToRenderer(screenBuffer_, "Object3D.Solid.BlendNormal");
        }

        waveNumbers_[i] = obj.get();
        ctx->AddObject3D(std::move(obj));
    }
}

void WaveSystem::TransitionToNextWave() {
    currentWaveIndex_++;

    if (currentWaveIndex_ >= static_cast<int>(waveDataList_.size())) {
        // 全Wave終了
        isAllWavesCompleted_ = true;
        isSystemRunning_ = false;
        isWaitingForNextWave_ = false;

        // カウントダウンを非表示
        HideCountdown();

        // Wave表示はアニメーション完了時に非表示になる

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
    isDurationExceeded_ = false;
    
    // 敵追跡情報をリセット
    currentWaveSpawnedEnemyCount_ = 0;
    currentWaveEnemyIDs_.clear();

    // アニメーションはWave終了時に開始済み
}

void WaveSystem::SpawnEnemyInternal(int mapX, int mapZ, EnemyType enemyType) {
    if (!enemyManager_) return;

    Vector3 position = MapToWorldPosition(mapX, mapZ);
    EnemyDirection direction = DetermineDirection(mapX, mapZ);

    int enemyID = enemyManager_->SpawnEnemy(enemyType, direction, position);
    
    // 生成した敵のIDを記録
    if (enemyID >= 0) {
        currentWaveEnemyIDs_.push_back(enemyID);
        currentWaveSpawnedEnemyCount_++;
    }
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

            // この位置にあるBombを削除
            if (bombManager_) {
                bombManager_->RemoveBombAtPosition(position);
            }

            // この位置のWallを非アクティブ化
            if (walls_ && mapW_ > 0 && mapH_ > 0) {
                const int gridX = static_cast<int>(std::round(position.x / 2.0f));
                const int gridZ = static_cast<int>(std::round(position.z / 2.0f));
                if (gridX >= 0 && gridX < mapW_ && gridZ >= 0 && gridZ < mapH_) {
                    const int index = gridZ * mapW_ + gridX;
                    if (walls_[index].isActive || walls_[index].isMoving) {
                        walls_[index].isActive = false;
                        walls_[index].isMoving = false;
                        walls_[index].moveTimer.Reset();
                        walls_[index].hp = 0;
                    }
                }
            }

            activeEmitPositions_.push_back(position);
            spawn.particleSpawned = true;
        }

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

void WaveSystem::UpdateCountdown() {
    if (!isWaitingForWaveStart_ || !bpmSystem_) {
        return;
    }

    // 残り拍数を計算
    int remainingBeats = preWaveDelayBeats_ - delayBeatCount_;
    
    if (remainingBeats > 0 && remainingBeats <= 9) {
        ShowCountdownNumber(remainingBeats);
        
        // BPM進行度を各数字モデルに設定
        for (int i = 0; i < kMaxCountdownNumbers_; ++i) {
            if (countdownNumbers_[i]) {
                if (auto* bpmScaling = countdownNumbers_[i]->GetComponent3D<BPMScaling>()) {
                    bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
                }
            }
        }
    } else if (remainingBeats <= 0) {
        HideCountdown();
    }
}

void WaveSystem::ShowCountdownNumber(int number) {
    if (number < 0 || number >= kMaxCountdownNumbers_) {
        return;
    }

    // 現在表示中の数字と異なる場合のみ更新
    if (currentCountdownNumber_ != number) {
        // 全ての数字を透明化
        for (int i = 0; i < kMaxCountdownNumbers_; ++i) {
            if (countdownNumbers_[i]) {
                if (auto* mt = countdownNumbers_[i]->GetComponent3D<Material3D>()) {
                    Vector4 color = mt->GetColor();
                    color.w = 0.0f;  // Alpha = 0
                    mt->SetColor(color);
                }
            }
        }

        // 新しい数字を表示（不透明化）
        if (countdownNumbers_[number]) {
            if (auto* mt = countdownNumbers_[number]->GetComponent3D<Material3D>()) {
                Vector4 color = mt->GetColor();
                color.w = 1.0f;  // Alpha = 1
                mt->SetColor(color);
            }
        }

        currentCountdownNumber_ = number;
    }
}

void WaveSystem::HideCountdown() {
    // 全ての数字を透明化
    for (int i = 0; i < kMaxCountdownNumbers_; ++i) {
        if (countdownNumbers_[i]) {
            if (auto* mt = countdownNumbers_[i]->GetComponent3D<Material3D>()) {
                Vector4 color = mt->GetColor();
                color.w = 0.0f;  // Alpha = 0
                mt->SetColor(color);
            }
        }
    }
    currentCountdownNumber_ = -1;
}

void WaveSystem::UpdateWaveDisplay() {
    // Wave切り替えアニメーション中は通常の表示処理をスキップ
    if (waveTransitionState_ != WaveTransitionState::Idle) {
        // アニメーション中はBPMスケーリングのみ更新
        if (bpmSystem_) {
            for (int i = 0; i < kMaxWaveNumbers_; ++i) {
                if (waveNumbers_[i]) {
                    if (auto* bpmScaling = waveNumbers_[i]->GetComponent3D<BPMScaling>()) {
                        bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
                    }
                }
            }
        }
        return;
    }

    // Wave開始待機中またはWave進行中のみ表示
    if (isWaitingForWaveStart_ || isWaveStarted_) {
        int waveNumber = currentWaveIndex_ + 1;
        if (waveNumber >= 1 && waveNumber <= kMaxWaveNumbers_) {
            ShowWaveNumber(waveNumber);
            
            // BPM進行度を各Waveモデルに設定
            if (bpmSystem_) {
                for (int i = 0; i < kMaxWaveNumbers_; ++i) {
                    if (waveNumbers_[i]) {
                        if (auto* bpmScaling = waveNumbers_[i]->GetComponent3D<BPMScaling>()) {
                            bpmScaling->SetBPMProgress(bpmSystem_->GetBeatProgress());
                        }
                    }
                }
            }
        }
    } else if (waveTransitionState_ == WaveTransitionState::Idle) {
        // アニメーション中でない場合のみ非表示
        HideWaveDisplay();
    }
}

void WaveSystem::ShowWaveNumber(int waveNumber) {
    if (waveNumber < 1 || waveNumber > kMaxWaveNumbers_) {
        return;
    }

    int index = waveNumber - 1;  // 配列は0始まり

    // 現在表示中のWaveと異なる場合のみ更新
    if (currentDisplayedWave_ != waveNumber) {
        // 全てのWave表示を透明化
        for (int i = 0; i < kMaxWaveNumbers_; ++i) {
            if (waveNumbers_[i]) {
                if (auto* mt = waveNumbers_[i]->GetComponent3D<Material3D>()) {
                    Vector4 color = mt->GetColor();
                    color.w = 0.0f;  // Alpha = 0
                    mt->SetColor(color);
                }
            }
        }

        // 新しいWaveを表示（不透明化）
        if (waveNumbers_[index]) {
            if (auto* mt = waveNumbers_[index]->GetComponent3D<Material3D>()) {
                Vector4 color = mt->GetColor();
                color.w = 1.0f;  // Alpha = 1
                mt->SetColor(color);
            }
        }

        currentDisplayedWave_ = waveNumber;
    }
}

void WaveSystem::HideWaveDisplay() {
    // 全てのWave表示を透明化
    for (int i = 0; i < kMaxWaveNumbers_; ++i) {
        if (waveNumbers_[i]) {
            if (auto* mt = waveNumbers_[i]->GetComponent3D<Material3D>()) {
                Vector4 color = mt->GetColor();
                color.w = 0.0f;  // Alpha = 0
                mt->SetColor(color);
            }
        }
    }
    currentDisplayedWave_ = -1;
}

void WaveSystem::UpdateWaveTransitionAnimation() {
    if (waveTransitionState_ == WaveTransitionState::Idle) {
        return;
    }

    float deltaTime = GetDeltaTime();
    waveTransitionTimer_ += deltaTime;

    switch (waveTransitionState_) {
    case WaveTransitionState::MovingOut:
    {
        // 開始位置から終了位置へ移動（イージング付き）
        float t = std::min(waveTransitionTimer_ / kWaveTransitionDuration_, 1.0f);
        
        if (currentDisplayedWave_ >= 1 && currentDisplayedWave_ <= kMaxWaveNumbers_) {
            int index = currentDisplayedWave_ - 1;
            if (waveNumbers_[index]) {
                if (auto* tr = waveNumbers_[index]->GetComponent3D<Transform3D>()) {
                    // イージングを適用した補間
                    Vector3 position = MyEasing::Lerp(waveDisplayStartPosition_, waveDisplayEndPosition_, t, EaseType::EaseInBack);
                    Vector3 rotation = MyEasing::Lerp(waveDisplayStartRotate_, waveDisplayEndRotate_ + Vector3({ 6.28f,0.0f,0.0f }), t, EaseType::EaseInExpo);
                    tr->SetTranslate(position);
                    tr->SetRotate(rotation);
                }
            }
        }

        if (t >= 1.0f) {
            
            // 現在のWaveを非表示にして次のWaveを表示
            if (currentDisplayedWave_ >= 1 && currentDisplayedWave_ <= kMaxWaveNumbers_) {
                int index = currentDisplayedWave_ - 1;
                if (waveNumbers_[index]) {
                    if (auto* mt = waveNumbers_[index]->GetComponent3D<Material3D>()) {
                        Vector4 color = mt->GetColor();
                        color.w = 0.0f;
                        mt->SetColor(color);
                    }
                }
            }

            // 次のWaveを表示
            if (nextWaveToDisplay_ >= 1 && nextWaveToDisplay_ <= kMaxWaveNumbers_) {
                int index = nextWaveToDisplay_ - 1;
                if (waveNumbers_[index]) {
                    // 終了位置に配置
                    if (auto* tr = waveNumbers_[index]->GetComponent3D<Transform3D>()) {
                        tr->SetTranslate(waveDisplayEndPosition_);
                        tr->SetRotate(waveDisplayEndRotate_);
                    }

                    // 表示
                    if (auto* mt = waveNumbers_[index]->GetComponent3D<Material3D>()) {
                        Vector4 color = mt->GetColor();
                        color.w = 1.0f;
                        mt->SetColor(color);
                    }
                }
                currentDisplayedWave_ = nextWaveToDisplay_;
            } else {
                currentDisplayedWave_ = -1;
            }

            // 移動完了、待機状態へ
            waveTransitionState_ = WaveTransitionState::WaitingToSwitch;
            waveTransitionTimer_ = 0.0f;
        }
        break;
    }

    case WaveTransitionState::WaitingToSwitch:
    {
        // 0.5秒待機
        if (waveTransitionTimer_ >= kWaveSwitchDelay_) {
            // 待機完了、Wave切り替えへ
            waveTransitionState_ = WaveTransitionState::SwitchingWave;
            waveTransitionTimer_ = 0.0f;
        }
        break;
    }

    case WaveTransitionState::SwitchingWave:
    {
        // 戻るアニメーションへ
        waveTransitionState_ = WaveTransitionState::MovingIn;
        waveTransitionTimer_ = 0.0f;
        break;
    }

    case WaveTransitionState::MovingIn:
    {
        // 終了位置から開始位置へ移動（イージング付き）
        float t = std::min(waveTransitionTimer_ / kWaveTransitionDuration_, 1.0f);
        
        if (currentDisplayedWave_ >= 1 && currentDisplayedWave_ <= kMaxWaveNumbers_) {
            int index = currentDisplayedWave_ - 1;
            if (waveNumbers_[index]) {
                if (auto* tr = waveNumbers_[index]->GetComponent3D<Transform3D>()) {
                    // イージングを適用した補間（戻りは滑らかに減速）
                    Vector3 position = MyEasing::Lerp(waveDisplayEndPosition_, waveDisplayStartPosition_, t, EaseType::EaseOutExpo);
                    Vector3 rotation = MyEasing::Lerp(waveDisplayEndRotate_, waveDisplayStartRotate_, t, EaseType::EaseOutExpo);
                    tr->SetTranslate(position);
                    tr->SetRotate(rotation);
                }
            }
        }

        if (t >= 1.0f) {
            // アニメーション完了
            waveTransitionState_ = WaveTransitionState::Idle;
            waveTransitionTimer_ = 0.0f;
            
            // 全Wave終了時は非表示
            if (nextWaveToDisplay_ == -1) {
                HideWaveDisplay();
            }
        }
        break;
    }

    default:
        break;
    }
}

bool WaveSystem::IsParticleEmittingAt(const Vector3& position) const {
    // 位置の許容誤差（グリッドの半分程度）
    const float tolerance = tileSize_ * 0.5f;

    for (const auto& emitPos : activeEmitPositions_) {
        float dx = std::abs(emitPos.x - position.x);
        float dz = std::abs(emitPos.z - position.z);

        // XZ平面で近い位置にあるかチェック（Y座標は無視）
        if (dx < tolerance && dz < tolerance) {
            return true;
        }
    }
    return false;
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

        ImGui::Separator();
        ImGui::Text("Countdown Settings");
        ImGui::DragFloat3("Countdown Position", &countdownPosition_.x, 0.1f);
        ImGui::DragFloat("Countdown Scale", &countdownScale_, 0.1f, 0.1f, 10.0f);
        ImGui::Text("Current Countdown Number: %d", currentCountdownNumber_);

        ImGui::Separator();
        ImGui::Text("Wave Display Settings");
        ImGui::DragFloat3("Wave Display Position", &waveDisplayPosition_.x, 0.1f);
        ImGui::DragFloat("Wave Display Scale", &waveDisplayScale_, 0.1f, 0.1f, 10.0f);
        ImGui::Text("Current Displayed Wave: %d", currentDisplayedWave_);

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
