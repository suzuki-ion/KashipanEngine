#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <memory>

#include "Objects/Components/Player/PlayerDrection.h"

namespace KashipanEngine {

// Forward declaration
class ExplosionManager;

/// @brief プレイヤーが生成するBombを一括管理するクラス
class BombManager final : public ISceneComponent {
public:
    /// @brief コンストラクタ
    /// @param maxBombs 同時に設置可能な爆弾の最大数
    explicit BombManager(int maxBombs = 6);
    ~BombManager() override = default;

    void Initialize() override;
    void Update() override;

    /// @brief プレイヤーオブジェクトを設定
    void SetPlayer(Object3DBase* player) { player_ = player; }

    /// @brief ScreenBufferを設定（爆弾のレンダリング用）
    void SetScreenBuffer(ScreenBuffer* screenBuffer) { screenBuffer_ = screenBuffer; }

    /// @brief ShadowMapBufferを設定（爆弾の影用）
    void SetShadowMapBuffer(ShadowMapBuffer* shadowMapBuffer) { shadowMapBuffer_ = shadowMapBuffer; }

    /// @brief ExplosionManagerを設定
    void SetExplosionManager(ExplosionManager* explosionManager) { explosionManager_ = explosionManager; }

	/// @brief InputCommandを設定
	void SetInputCommand(const InputCommand* inputCommand) { inputCommand_ = inputCommand; }

    /// @brief 衝突判定用ColliderComponentを設定
    void SetCollider(ColliderComponent* collider) { collider_ = collider; }

    /// @brief BPM進行度を設定（0.0～1.0）
    void SetBPMProgress(float progress) { bpmProgress_ = progress; }

    /// @brief BPMの許容範囲を設定
    void SetBPMToleranceRange(float range) { bpmToleranceRange_ = range; }

    /// @brief 1拍の時間を設定（秒）
    void SetBeatDuration(float duration) { beatDuration_ = duration; }

    /// @brief 爆弾の最大数を設定
    void SetMaxBombs(int maxBombs) { maxBombs_ = maxBombs; }

    /// @brief 爆弾の生成距離を設定
    void SetSpawnDistance(float distance) { spawnDistance_ = distance; }

    /// @brief 爆弾のスケールを設定
    void SetBombScale(float scale) { bombScale_ = scale; }

    /// @brief 爆弾の寿命を設定（拍数）
    void SetBombLifetimeBeats(int beats) { bombLifetimeBeats_ = beats; }

    /// @brief マップサイズを設定
    void SetMapSize(int width, int height) { mapWidth_ = width; mapHeight_ = height; }

    /// @brief 現在設置されている爆弾の数を取得
    int GetActiveBombCount() const { return static_cast<int>(activeBombs_.size()); }

    /// @brief 爆発範囲内のボムを起爆する
    /// @param explosionCenter 爆発の中心位置
    /// @param explosionRange 爆発の範囲（十字の長さ）
    void DetonateBombsInExplosionRange(const Vector3& explosionCenter, float explosionRange);

    /// @brief 指定位置にボムがあるか（プレイヤー移動のブロック判定用）
    bool IsBombAtPosition(const Vector3& position) const { return HasBombAtPosition(position); }

    /// @brief Enemyが爆弾に当たった時の処理
    /// @param hitObject 当たったオブジェクト
    void OnEnemyHit(Object3DBase* hitObject);

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    /// @brief 爆弾を生成
    void SpawnBomb();

    /// @brief プレイヤーの向いている方向のオフセットを取得
    Vector3 GetDirectionOffset(PlayerDirection direction) const;

    /// @brief 指定位置がマップ内かどうかをチェック
    bool IsInsideMap(const Vector3& position) const;

    /// @brief 指定位置に既に爆弾があるかどうかをチェック
    bool HasBombAtPosition(const Vector3& position) const;

    /// @brief 爆弾情報
    struct BombInfo {
        Object3DBase* object = nullptr;
        float elapsedTime = 0.0f;             // 経過時間（秒）
        int elapsedBeats = 0;                 // 経過拍数
        float beatAccumulator = 0.0f;         // ビートの蓄積（0.0～1.0で1ビート）
        Vector3 position{ 0.0f, 0.0f, 0.0f }; // 爆弾の位置（重複チェック用）
        bool shouldDetonate = false;          // 起爆フラグ
    };

    std::vector<BombInfo> activeBombs_;

    Object3DBase* player_ = nullptr;
    ScreenBuffer* screenBuffer_ = nullptr;
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;
    const InputCommand* inputCommand_ = nullptr;
    ExplosionManager* explosionManager_ = nullptr;
    ColliderComponent* collider_ = nullptr;

    int maxBombs_ = 10;
    float bpmProgress_ = 0.0f;
    float prevBpmProgress_ = 0.0f;   // 前フレームのBPM進行度（ビートカウント用）
    float bpmToleranceRange_ = 0.2f;
    bool useToleranceRange_ = true;
    float beatDuration_ = 0.5f;      // 1拍の時間（秒）
    float spawnDistance_ = 2.0f;
    float bombScale_ = 0.8f;
    int bombLifetimeBeats_ = 4;      // 爆弾の寿命（拍数）

    int mapWidth_ = 13;              // マップの横幅（グリッド数）
    int mapHeight_ = 13;             // マップの縦幅（グリッド数）
};

} // namespace KashipanEngine