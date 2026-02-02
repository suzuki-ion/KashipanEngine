#pragma once
#include "EnemyType.h"
#include <KashipanEngine.h>
#include <memory>
#include <vector>
#include "Scenes/Components/BPM/BPMSystem.h"
#include "Objects/Components/Bomb/BombManager.h"
#include "Objects/Components/ParticleConfig.h"
#include "Objects/Components/Map/WallInfo.h"

namespace KashipanEngine {

// 敵の死因を表す列挙型
enum class EnemyDeathCause {
    OutOfBounds,  // 場外で死亡
    Explosion,    // 爆発で死亡
	Area,         // 中心エリアで死亡
	Collision     // 吹き飛んだ敵と衝突で死亡
};

class EnemyManager final : public ISceneComponent {
public:
    explicit EnemyManager()
        : ISceneComponent("EnemyManager", 100) {
    }

    ~EnemyManager() override = default;

    void Initialize() override;
    void Update() override;

    // 敵を生成
    int SpawnEnemy(EnemyType type, EnemyDirection direction, const Vector3& position);

	void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

	void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

    void SetBPMSystem(BPMSystem* bpmSystem) { bpmSystem_ = bpmSystem; }

    void SetMapSize(int width, int height) { mapW_ = width; mapH_ = height; }

    void SetCollider(ColliderComponent* collider) { collider_ = collider; }

    void SetPlayer(Object3DBase* player) { player_ = player; }

    void SetBombManager(BombManager* bombManager) { bombManager_ = bombManager; }

    // ScoreManagerを設定
    void SetScoreManager(class ScoreManager* scoreManager) { scoreManager_ = scoreManager; }

    // ScoreDisplayを設定
    void SetScoreDisplay(class ScoreDisplay* scoreDisplay) { scoreDisplay_ = scoreDisplay; }

    /// @brief 壁配列を設定する
    /// @param walls 壁配列の先頭ポインタ
    /// @param width マップの横幅
    /// @param height マップの縦幅
    void SetWalls(WallInfo* walls, int width, int height) { 
        walls_ = walls; 
        wallsWidth_ = width;
        wallsHeight_ = height;
    }

    /// @brief 爆発が敵に当たった時の処理
    /// @param hitObject 当たったオブジェクト
    /// @param explosionCenter 爆発の中心位置
    void OnExplosionHit(Object3DBase* hitObject, const Vector3& explosionCenter);

    /// @brief 敵が倒された時のコールバックを設定
    /// @param callback 倒された時に呼ばれるコールバック関数
    void SetOnEnemyDestroyedCallback(std::function<void()> callback) {
        onEnemyDestroyedCallback_ = callback;
    }

    /// @brief 爆発開始時のコールバックを設定
    /// @param callback 爆発開始時に呼ばれるコールバック関数
    void SetOnExplosionStartCallback(std::function<void()> callback) {
        onExplosionStartCallback_ = callback;
    }

    /// @brief すべての敵を消去する
    void ClearAllEnemies();

    /// @brief 敵が生存しているかチェック
    /// @param enemyID 敵のID（activeEnemiesのインデックス）
    /// @return 生存している場合true
    bool IsEnemyAlive(int enemyID) const;

    // パーティクルプールを初期化
    void InitializeParticlePool();

    void SetDieParticleConfig(const ParticleConfig& config) {
        dieParticleConfig_ = config;
	}
private:

    void CleanupDeadEnemies();
    
    // 死亡パーティクルを発生させる
    void SpawnDieParticles(const Vector3& position);

    /// @brief 指定したマップ座標に壁があるかチェック
    /// @param x マップX座標
    /// @param z マップZ座標
    /// @return 壁がある場合true
    bool IsWallAt(int x, int z) const;

    /// @brief 指定したマップ座標の壁にダメージを与える
    /// @param x マップX座標
    /// @param z マップZ座標
    void DamageWallAt(int x, int z);

    /// @brief 指定したマップ座標の壁を破壊する
    /// @param x マップX座標
    /// @param z マップZ座標
    void DestroyWallAt(int x, int z);

    /// @brief 指定したマップ座標が中心から伸びる十字エリアかチェック
    /// @param x マップX座標
    /// @param z マップZ座標
    /// @return 十字エリアの場合true
    bool IsCrossAreaFromCenter(int x, int z) const;

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
        
        // 移動状態の追加
        bool isMovingToCenter = false;  // 中心エリアに向かって移動中かどうか
        bool isReversedFromWall = false;  // 壁で反転したフラグ（中心に向かわない）
        
        // 吹き飛び関連
        bool isKnockedBack = false;  // 吹き飛び中かどうか
        Vector3 knockbackVelocity{ 0.0f, 0.0f, 0.0f };  // 吹き飛び速度
        float knockbackTimer = 0.0f;  // 吹き飛び経過時間
        
        // 死因の追加
        EnemyDeathCause deathCause = EnemyDeathCause::OutOfBounds;  // 死因（デフォルトは場外）
    };

    std::vector<EnemyInfo> activeEnemies_;
    
    std::function<void()> onEnemyDestroyedCallback_;
    std::function<void()> onExplosionStartCallback_;

    ParticleConfig dieParticleConfig_{};

    // パーティクルプール
    std::vector<Object3DBase*> particlePool_;
    static constexpr int kParticlePoolSize_ = 50;
    
    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    ColliderComponent* collider_ = nullptr;
    Object3DBase* player_ = nullptr;
    BombManager* bombManager_ = nullptr;
    class ScoreManager* scoreManager_ = nullptr;  // スコアマネージャー
    class ScoreDisplay* scoreDisplay_ = nullptr;  // スコア表示

    WallInfo* walls_ = nullptr;
    int wallsWidth_ = 0;
    int wallsHeight_ = 0;

    BPMSystem* bpmSystem_ = nullptr;
    int lastMoveBeat_ = -1;

    int mapW_ = 0;
    int mapH_ = 0;

    float moveDistance_ = 2.0f;

	float bpmProgress_ = 0.0f;

	float dieVolume_ = 0.1f;
    
    // 吹き飛び設定
    float knockbackSpeed_ = 10.0f;     // 吹き飛び速度（より速く）
    
    // マップ中心の設定
    std::vector<std::pair<int, int>> centerPositions_ = {
        {4, 4}, {5, 4}, {4, 5}, {5, 5}
    };
};

} // namespace KashipanEngine