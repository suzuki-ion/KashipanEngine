#pragma once

#include "KashipanEngine.h"
#include "PlayerDrection.h"
#include "../BPMScaling.h"

namespace KashipanEngine {

/// @brief 爆弾設置リクエスト情報
struct BombPlacementRequest {
    Vector3 position;      ///< 設置位置（ワールド座標）
    int mapX;              ///< マップX座標
    int mapZ;              ///< マップZ座標
    float scale;           ///< 爆弾のスケール
};

/// @brief プレイヤーの向いている方向にBPMに合わせて爆弾を設置するコンポーネント
/// @note このコンポーネントは爆弾設置のリクエスト生成のみを担当し、実際の爆弾管理はBombManagerが行う
class BombSpawn final : public IObjectComponent3D {
public:
    /// @brief コンストラクタ
    /// @param maxBombs 設置可能な最大爆弾数
    explicit BombSpawn(int maxBombs = 3)
        : IObjectComponent3D("BombSpawn", 1)
        , maxBombs_(maxBombs) {}

    ~BombSpawn() override = default;

    std::unique_ptr<IObjectComponent> Clone() const override {
        auto clone = std::make_unique<BombSpawn>(maxBombs_);
        clone->bombOffset_ = bombOffset_;
        clone->bombScale_ = bombScale_;
        clone->bpmToleranceRange_ = bpmToleranceRange_;
        return clone;
    }

    std::optional<bool> Update() override;

    /// @brief 爆弾設置を試みる（リクエストを生成）
    /// @return 設置リクエストが生成された場合はその情報、できない場合はnull
    std::optional<BombPlacementRequest> TryCreatePlacementRequest();

    /// @brief プレイヤーの向きから爆弾設置位置を計算
    /// @param currentPos プレイヤーの現在位置
    /// @param direction プレイヤーの向き
    /// @return 爆弾設置位置
    Vector3 CalculateBombPosition(const Vector3& currentPos, PlayerDirection direction) const;

    /// @brief マップ座標が有効範囲内かチェック
    /// @param mapX マップX座標
    /// @param mapZ マップZ座標
    /// @return 有効範囲内ならtrue
    bool IsValidMapPosition(int mapX, int mapZ) const;

    /// @brief ワールド座標からマップ座標を取得
    /// @param worldPos ワールド座標
    /// @param outMapX マップX座標の出力先
    /// @param outMapZ マップZ座標の出力先
    void WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const;

    /// @brief 爆弾が設置されたことを通知（外部から呼ばれる）
    void OnBombPlaced() { currentBombCount_++; }

    /// @brief 爆弾が削除されたことを通知（外部から呼ばれる）
    void OnBombRemoved() { 
        if (currentBombCount_ > 0) {
            currentBombCount_--;
        }
    }

    /// @brief すべての爆弾がクリアされたことを通知
    void OnAllBombsCleared() { currentBombCount_ = 0; }

    /// @brief 現在設置されている爆弾の数を取得
    int GetBombCount() const { return currentBombCount_; }

    /// @brief 設置可能な最大爆弾数を設定
    void SetMaxBombs(int maxBombs) { maxBombs_ = maxBombs; }

    /// @brief 設置可能な最大爆弾数を取得
    int GetMaxBombs() const { return maxBombs_; }

    /// @brief 爆弾設置位置のオフセット距離を設定
    void SetBombOffset(float offset) { bombOffset_ = offset; }

    /// @brief 爆弾設置位置のオフセット距離を取得
    float GetBombOffset() const { return bombOffset_; }

    /// @brief 爆弾のスケールを設定
    void SetBombScale(float scale) { bombScale_ = scale; }

    /// @brief 爆弾のスケールを取得
    float GetBombScale() const { return bombScale_; }

    /// @brief BPM進行度の設定（外部から更新される）
    void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

    /// @brief BPM進行度を取得
    float GetBPMProgress() const { return bpmProgress_; }

    /// @brief BPMの許容範囲の設定
    void SetBPMToleranceRange(float range) { bpmToleranceRange_ = range; }

    /// @brief BPMの許容範囲を取得
    float GetBPMToleranceRange() const { return bpmToleranceRange_; }

    /// @brief 入力システムの設定
    void SetInput(const Input* input) { input_ = input; }

    /// @brief マップサイズの設定
    void SetMapSize(int width, int height) {
        mapWidth_ = width;
        mapHeight_ = height;
    }

    /// @brief マップサイズを取得
    void GetMapSize(int& outWidth, int& outHeight) const {
        outWidth = mapWidth_;
        outHeight = mapHeight_;
    }

    /// @brief クリアリクエストが発行されたかチェック
    bool HasClearRequest() const { return clearRequested_; }

    /// @brief クリアリクエストを消費
    void ConsumeClearRequest() { clearRequested_ = false; }

#if defined(USE_IMGUI)
    void ShowImGui() override;
#endif

private:
    int maxBombs_ = 3;                      ///< 設置可能な最大爆弾数
    int currentBombCount_ = 0;              ///< 現在設置されている爆弾数
    float bombOffset_ = 2.0f;               ///< 爆弾設置位置のオフセット距離
    float bombScale_ = 1.0f;                ///< 爆弾のスケール
    float bpmProgress_ = 0.0f;              ///< BPMに同期した進行度（0.0～1.0）
    float bpmToleranceRange_ = 0.2f;        ///< BPM進行度の許容範囲
    int mapWidth_ = 13;                     ///< マップの幅
    int mapHeight_ = 13;                    ///< マップの高さ
    bool clearRequested_ = false;           ///< クリアリクエストフラグ
    
    const Input* input_ = nullptr;          ///< 入力システムへのポインタ
};

} // namespace KashipanEngine} // namespace KashipanEngine