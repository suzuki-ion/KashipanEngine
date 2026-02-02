#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <functional>
#include "Scenes/Components/BPM/BPMSystem.h"
#include "Objects/Components/Enemy/EnemyManager.h"
#include "Objects/Components/Enemy/EnemySpawner.h"
#include "Objects/Components/Enemy/EnemyType.h"
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

/// @brief Waveの種類を定義する列挙型
enum class Wave {
    Wave1,
    Wave2,
    Wave3,
    Wave4,
    Wave5,
    Count  // Waveの総数
};

/// @brief 敵のスポーン情報を保持する構造体
struct EnemySpawnData {
    int spawnBeat;        // Wave内の生成される拍数
    int mapX;             // マップX座標
    int mapZ;             // マップZ座標
    EnemyType enemyType;  // 敵の種類
};

/// @brief Wave情報を保持する構造体
struct WaveData {
    Wave wave;                               // Waveの種類
    int duration;                            // Waveの持続時間（拍数）
    std::vector<EnemySpawnData> spawnList;   // このWaveで生成する敵のリスト
};

/// @brief Waveシステムを管理するコンポーネント
class WaveSystem final : public ISceneComponent {
public:
    explicit WaveSystem()
        : ISceneComponent("WaveSystem", 1) {
    }

    ~WaveSystem() override = default;

    void Initialize() override;
    void Update() override;

    /// @brief BPMSystemを設定
    void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }

    /// @brief EnemyManagerを設定
    void SetEnemyManager(EnemyManager* enemyManager) { enemyManager_ = enemyManager; }

    /// @brief EnemySpawnerを設定（SpawnParticle用）
    void SetEnemySpawner(EnemySpawner* enemySpawner) { enemySpawner_ = enemySpawner; }

    /// @brief ScreenBufferを設定
    void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

    /// @brief Wave開始前の待機拍数を設定
    void SetPreWaveDelay(int beats) { preWaveDelayBeats_ = beats; }

    /// @brief Wave終了後の待機拍数を設定
    void SetPostWaveDelay(int beats) { postWaveDelayBeats_ = beats; }

    /// @brief SpawnParticle発生から敵生成までの拍数を設定
    void SetSpawnParticleLeadBeats(int beats) { spawnParticleLeadBeats_ = beats; }

    /// @brief Waveデータを追加
    void AddWaveData(const WaveData& waveData);

    /// @brief Waveデータをクリア
    void ClearWaveData();

    /// @brief システムをリセット
    void ResetSystem();

    /// @brief システムを開始
    void StartSystem();

    /// @brief システムを停止
    void StopSystem();

    /// @brief 現在のWaveを取得
    Wave GetCurrentWave() const { return currentWave_; }

    /// @brief Wave内の経過拍数を取得
    int GetWaveBeatCount() const { return waveBeatCount_; }

    /// @brief Waveが開始しているかを取得
    bool IsWaveStarted() const { return isWaveStarted_; }

    /// @brief 全Waveが終了したかを取得
    bool IsAllWavesCompleted() const { return isAllWavesCompleted_; }

    /// @brief システムが動作中かを取得
    bool IsSystemRunning() const { return isSystemRunning_; }

    /// @brief 指定タイミングで敵を生成する関数を追加
    /// @param beatInWave Wave内の生成される拍数
    /// @param mapX マップX座標
    /// @param mapZ マップZ座標
    /// @param enemyType 敵の種類
    void ScheduleEnemySpawn(int beatInWave, int mapX, int mapZ, EnemyType enemyType);

    /// @brief パーティクル設定を設定
    void SetSpawnParticleConfig(const ParticleConfig& config) { spawnParticleConfig_ = config; }

    /// @brief 全Wave終了時のコールバックを設定
    void SetOnAllWavesCompletedCallback(std::function<void()> callback) {
        onAllWavesCompletedCallback_ = callback;
    }

    /// @brief パーティクルプールを初期化
    void InitializeParticlePool(int particlesPerFrame = 1);

    /// @brief カウントダウン用のNumberModelプールを初期化
    void InitializeCountdownModels();

	void SetParentTransform(Transform3D* parent) { parentTransform_ = parent; }
#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    /// @brief 次のWaveへ移行
    void TransitionToNextWave();

    /// @brief 敵を生成（内部処理）
    void SpawnEnemyInternal(int mapX, int mapZ, EnemyType enemyType);

    /// @brief SpawnParticleを発生させる
    void SpawnParticlesAtPosition(const Vector3& position, int count = 1);

    /// @brief マップ座標からワールド座標を計算
    Vector3 MapToWorldPosition(int mapX, int mapZ) const;

    /// @brief マップ座標から敵の初期方向を決定
    EnemyDirection DetermineDirection(int mapX, int mapZ) const;

    /// @brief 予約されたスポーンを処理
    void ProcessScheduledSpawns();

    /// @brief 予約されたパーティクルを処理
    void ProcessScheduledParticles();

    /// @brief カウントダウン表示を更新
    void UpdateCountdown();

    /// @brief カウントダウンの数字を表示
    void ShowCountdownNumber(int number);

    /// @brief カウントダウンを非表示
    void HideCountdown();

    Transform3D* parentTransform_ = nullptr;

    BPMSystem* bpmSystem_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    EnemySpawner* enemySpawner_ = nullptr;
    ScreenBuffer* screenBuffer_ = nullptr;
   
    // Wave管理
    std::vector<WaveData> waveDataList_;
    Wave currentWave_ = Wave::Wave1;
    int currentWaveIndex_ = 0;

    // 拍数カウント
    int globalBeatCount_ = 0;     // システム開始からの総拍数
    int waveBeatCount_ = 0;       // 現在のWave内の経過拍数
    int delayBeatCount_ = 0;      // 待機中の拍数カウント
    int lastProcessedBeat_ = -1;  // 最後に処理した拍

    // 状態フラグ
    bool isSystemRunning_ = false;      // システムが動作中か
    bool isWaveStarted_ = false;        // Waveが開始しているか
    bool isWaitingForWaveStart_ = true; // Wave開始待機中か
    bool isWaitingForNextWave_ = false; // 次のWave待機中か
    bool isAllWavesCompleted_ = false;  // 全Waveが終了したか

    // 設定
    int preWaveDelayBeats_ = 4;         // Wave開始前の待機拍数
    int postWaveDelayBeats_ = 4;        // Wave終了後の待機拍数
    int spawnParticleLeadBeats_ = 3;    // SpawnParticle発生から敵生成までの拍数

    // マップ設定
    int mapWidth_ = 10;
    int mapHeight_ = 10;
    float tileSize_ = 2.0f;

    // パーティクル関連
    ParticleConfig spawnParticleConfig_{};
    std::vector<Object3DBase*> particlePool_;
    int particlesPerFrame_ = 1;
    static constexpr int kParticlePoolSize_ = 100;

    // スポーン予約管理
    struct ScheduledSpawn {
        int targetBeat;   // 生成する拍数
        int mapX;
        int mapZ;
        EnemyType enemyType;
        bool particleSpawned;  // パーティクルを発生させたか
    };
    std::vector<ScheduledSpawn> scheduledSpawns_;

    // パーティクル放出中の位置リスト（EnemySpawnerと同じ挙動のため）
    std::vector<Vector3> activeEmitPositions_;

    // カウントダウン表示用
    static constexpr int kMaxCountdownNumbers_ = 10;  // 0-9の数字
    std::array<Object3DBase*, kMaxCountdownNumbers_> countdownNumbers_{};
    int currentCountdownNumber_ = -1;  // 現在表示中の数字（-1は非表示）
    Vector3 countdownPosition_{ 0.0f, 0.0f, 15.0f };  // カウントダウン表示位置
    float countdownScale_ = 3.0f;  // カウントダウンのスケール

    // コールバック
    std::function<void()> onAllWavesCompletedCallback_;
};

} // namespace KashipanEngine
