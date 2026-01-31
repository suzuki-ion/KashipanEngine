#pragma once
#include <KashipanEngine.h>
#include "EnemyType.h"
#include "EnemyManager.h"
#include "EnemySpawnParticle.h"
#include <Scenes/Components/BPM/BPMSystem.h>
#include <Objects/Components/Bomb/BombManager.h>
#include <Objects/Components/Map/WallInfo.h>
#include <vector>
#include <random>
#include "Objects/Components/ParticleConfig.h"

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

        /// @brief BombManagerを設定（スポーン位置から爆弾を除外するため）
        void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

        /// @brief 壁情報を設定（スポーン位置から壁を除外するため）
        void SetWalls(WallInfo* walls, int mapWidth, int mapHeight) {
            walls_ = walls;
            mapWidth_ = mapWidth;
            mapHeight_ = mapHeight;
        }

        // パーティクルプールを初期化
        void InitializeParticlePool(int particlesPerFrame = 1);

        // スポーンパーティクル設定
        void SetSpawnParticleConfig(const ParticleConfig& config) { particleConfig_ = config; }

        void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }
        void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

        void SetEnemyDieParticleConfig(const ParticleConfig& config) { particleConfig_ = config; }

		void SetIsStarted(bool isStarted) { isStarted_ = isStarted; }
#if defined(USE_IMGUI)
        void ShowImGui() override;
#endif

    private:
        void SpawnEnemy();
        void SpawnParticlesAtPosition(const Vector3& position);
        EnemyType DetermineEnemyType() const;
        EnemyDirection ChooseSpawnDirection_NoOutward(const Vector3& pos, int mapW = 13, int mapH = 13, float tile = 2.0f) const;
        const SpawnPoint& SelectSpawnPoint() const;

        /// @brief 指定位置に爆弾または壁が存在するかチェック
        bool IsPositionOccupied(const Vector3& position) const;

        /// @brief 有効なスポーン位置を取得（爆弾や壁を避ける）
        bool GetValidSpawnPosition(Vector3& outPosition);

        static constexpr int kParticlePoolSize_ = 50;

        std::vector<SpawnPoint> spawnPoints_;
        int spawnInterval_ = 4; // 何拍ごとにスポーンするか
        int lastSpawnBeat_ = -1;
        int preSpawnBeat_ = -1; // パーティクル発生用の拍数記録
        EnemyManager* enemyManager_ = nullptr;
        BPMSystem* bpmSystem_ = nullptr;
        BombManager* bombManager_ = nullptr;

        WallInfo* walls_ = nullptr;
        int mapWidth_ = 0;
        int mapHeight_ = 0;

        ScreenBuffer* screenBuffer_ = nullptr;
        ShadowMapBuffer* shadowMapBuffer_ = nullptr;

        int particlesPerFrame_ = 1; // 毎フレーム放出するパーティクル数

        // スポーンパーティクル関連（オブジェクトプーリング）
        ParticleConfig particleConfig_ = {};
        std::vector<Object3DBase*> particlePool_; // プールに変更
        Vector3 nextSpawnPosition_{ 0.0f, 0.0f, 0.0f }; // 次のスポーン位置
        bool isEmittingParticles_ = false;  // パーティクル放出中フラグ

		bool isStarted_ = false;
    };

} // namespace KashipanEngine