#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <memory>

#include "Objects/Components/Enemy/EnemyManager.h"

namespace KashipanEngine {

// Forward declaration
class BombManager;

/// @brief 爆発エフェクトを一括管理するクラス
class ExplosionManager final : public ISceneComponent {
public:
    /// @brief コンストラクタ
    ExplosionManager();
    ~ExplosionManager() override = default;

    void Initialize() override;
    void Update() override;

    /// @brief ScreenBufferを設定（爆発のレンダリング用）
    void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

    /// @brief ShadowMapBufferを設定（爆発の影用）
    void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

    /// @brief BombManagerを設定（爆発との衝突検出用）
    void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

    /// @brief EnemyManagerを設定（敵との衝突検出用）
    void SetEnemyManager(EnemyManager* enemyManager) { enemyManager_ = enemyManager; }

    /// @brief Playerを設定（プレイヤーとの衝突検出用）
    void SetPlayer(Object3DBase* player) { player_ = player; }

	/// @brief 衝突判定用ColliderComponentを設定
    void SetCollider(ColliderComponent* collider) { collider_ = collider; }
    void SetCollider2(ColliderComponent* collider) { collider2_ = collider; }

    /// @brief 指定位置に爆発を生成
    /// @param position 爆発を生成する位置
    void SpawnExplosion(const Vector3& position);

    /// @brief 爆発のスケールを設定
    void SetExplosionScale(float scale) { explosionScale_ = scale; }

    /// @brief 爆発の寿命を設定（秒）
    void SetExplosionLifetime(float lifetime) { explosionLifetime_ = lifetime; }

    /// @brief 現在アクティブな爆発の数を取得
    int GetActiveExplosionCount() const { return static_cast<int>(activeExplosions_.size()); }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    /// @brief 爆発情報
    struct ExplosionInfo {
        Model* object = nullptr;
        Model* object2 = nullptr;
        float elapsedTime = 0.0f;             // 経過時間（秒）
        Vector3 position{ 0.0f, 0.0f, 0.0f }; // 爆発の位置
    };

    /// @brief 爆発とボムの衝突をチェックして起爆させる
    void CheckExplosionBombCollisions();

    float size_ = 3.0f;
    float sizeMagnification_ = 1.5f;

    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    BombManager* bombManager_ = nullptr;
    EnemyManager* enemyManager_ = nullptr;
    Object3DBase* player_ = nullptr;
    ColliderComponent* collider_ = nullptr;
    ColliderComponent* collider2_ = nullptr;

    std::vector<ExplosionInfo> activeExplosions_;

    float explosionScale_ = 1.0f;
    float explosionLifetime_ = 0.5f;      // 爆発の寿命（秒）
};

} // namespace KashipanEngine
