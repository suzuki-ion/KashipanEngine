#pragma once
#include "EnemyType.h"
#include <KashipanEngine.h>
#include <memory>
#include <vector>
#include "Scenes/Components/BPM/BPMSystem.h"
#include "Objects/Components/Bomb/BombManager.h"
#include "Objects/Components/ParticleConfig.h"

namespace KashipanEngine {

class EnemyManager final : public ISceneComponent {
public:
    explicit EnemyManager()
        : ISceneComponent("EnemyManager", 100) {
    }

    ~EnemyManager() override = default;

    void Initialize() override;
    void Update() override;

    // 敵を生成
    void SpawnEnemy(EnemyType type, EnemyDirection direction, const Vector3& position);

	void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

	void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

    void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }

    void SetMapSize(int width, int height) { mapW_ = width; mapH_ = height; }

    void SetCollider(ColliderComponent* collider) { collider_ = collider; }

    void SetPlayer(Object3DBase* player) { player_ = player; }

    void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

    /// @brief 爆発が敵に当たった時の処理
    /// @param hitObject 当たったオブジェクト
    void OnExplosionHit(Object3DBase* hitObject);

    // パーティクルプールを初期化
    void InitializeParticlePool();

    void SetDieParticleConfig(const ParticleConfig& config) {
        dieParticleConfig_ = config;
	}
private:

    void CleanupDeadEnemies();
    
    // 死亡パーティクルを発生させる
    void SpawnDieParticles(const Vector3& position);

	/// @brief アクティブな敵情報
    struct EnemyInfo {
        Object3DBase* object = nullptr; 
		EnemyType type = EnemyType::Basic;
		EnemyDirection direction = EnemyDirection::Down;
        Vector3 position{ 0.0f, 0.0f, 0.0f };
		Vector3 startPosition{ 0.0f, 0.0f, 0.0f };   // 追加: 移動開始位置
		Vector3 targetPosition{ 0.0f, 0.0f, 0.0f };  // 追加: 移動目標位置
		bool isDead = false;
        int moveEveryNBeats = 2;  // 何拍ごとに移動するか（デフォルト: 1拍ごと）
    };

    std::vector<EnemyInfo> activeEnemies_;
    
	ParticleConfig dieParticleConfig_{};

    // パーティクルプール
    std::vector<Object3DBase*> particlePool_;
    static constexpr int kParticlePoolSize_ = 100;
    
    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    ColliderComponent* collider_ = nullptr;
    Object3DBase* player_ = nullptr;
    BombManager* bombManager_ = nullptr;

    BPMSystem* bpmSystem_ = nullptr;
    int lastMoveBeat_ = -1;

    int mapW_ = 0;
    int mapH_ = 0;

    float moveDistance_ = 2.0f;

	float bpmProgress_ = 0.0f;

	float dieVolume_ = 0.1f;
};

} // namespace KashipanEngine