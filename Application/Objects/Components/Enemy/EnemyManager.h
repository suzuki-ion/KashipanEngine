#pragma once
#include "EnemyType.h"
#include <KashipanEngine.h>
#include <memory>
#include <vector>
#include "Scenes/Components/BPM/BPMSystem.h"

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

    void SetCollider(ColliderComponent* collider) { collider_ = collider; }  // 追加

    void SetPlayer(Object3DBase* player) { player_ = player; }                // 追加

private:

    void CleanupDeadEnemies();

	/// @brief アクティブな敵情報
    struct EnemyInfo {
        Object3DBase* object = nullptr; 
		EnemyType type;                       // 敵の種類
		EnemyDirection direction;        // 敵の開始方向
        Vector3 position{ 0.0f, 0.0f, 0.0f }; // 爆弾の位置（重複チェック用）
		bool isDead = false;                  // 敵が死亡しているか
    };

    std::vector<EnemyInfo> activeEnemies_;
    
    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    ColliderComponent* collider_ = nullptr;
    Object3DBase* player_ = nullptr;  // 追加

    BPMSystem* bpmSystem_ = nullptr;
    int lastMoveBeat_ = -1;

    int mapW_ = 0;
    int mapH_ = 0;

    float moveDistance_ = 2.0f;
};

} // namespace KashipanEngine