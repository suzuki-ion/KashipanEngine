#pragma once

#include "KashipanEngine.h"
#include "PlayerDrection.h"
#include "../BPMScaling.h"

namespace KashipanEngine {

/// @brief プレイヤーの向いている方向にBPMに合わせて爆弾を設置するコンポーネント
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

    /// @brief 爆弾設置を試みる
    void TryPlaceBomb();

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

    /// @brief 指定したマップ座標に既にボムが存在するかチェック
    /// @param mapX マップX座標
    /// @param mapZ マップZ座標
    /// @return ボムが存在する場合はtrue
    bool IsBombAtPosition(int mapX, int mapZ) const;

    /// @brief ワールド座標からマップ座標を取得
    /// @param worldPos ワールド座標
    /// @param outMapX マップX座標の出力先
    /// @param outMapZ マップZ座標の出力先
    void WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const;

    /// @brief 爆弾オブジェクトを作成してシーンに追加
    /// @param position 爆弾の設置位置
    void CreateBomb(const Vector3& position);

    /// @brief 爆弾を削除（爆発時などに呼ばれる想定）
    /// @param index 削除する爆弾のインデックス
    void RemoveBomb(size_t index);

    /// @brief すべての爆弾をクリア
    void ClearAllBombs();

    /// @brief 現在設置されている爆弾の数を取得
    int GetBombCount() const { return static_cast<int>(bombObjects_.size()); }

    /// @brief 設置可能な最大爆弾数を設定
    void SetMaxBombs(int maxBombs) { maxBombs_ = maxBombs; }

    /// @brief 設置可能な最大爆弾数を取得
    int GetMaxBombs() const { return maxBombs_; }

    /// @brief 爆弾設置位置のオフセット距離を設定
    void SetBombOffset(float offset) { bombOffset_ = offset; }

    /// @brief 爆弾のスケールを設定
    void SetBombScale(float scale) { bombScale_ = scale; }

    /// @brief BPM進行度の設定（外部から更新される）
    void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

    /// @brief BPMの許容範囲の設定
    void SetBPMToleranceRange(float range) { bpmToleranceRange_ = range; }

    /// @brief 入力システムの設定
    void SetInput(const Input* input) { input_ = input; }

    /// @brief シーンとレンダーバッファの設定
    void SetScene(SceneBase* scene, ScreenBuffer* screenBuffer, ShadowMapBuffer* shadowMapBuffer) {
        scene_ = scene;
        screenBuffer_ = screenBuffer;
        shadowMapBuffer_ = shadowMapBuffer;
    }

    /// @brief マップサイズの設定
    void SetMapSize(int width, int height) {
        mapWidth_ = width;
        mapHeight_ = height;
    }

    /// @brief 設置されている爆弾オブジェクトのリストを取得
    const std::vector<Object3DBase*>& GetBombObjects() const { return bombObjects_; }

    void ShowImGui() override;

private:
    int maxBombs_ = 3;                      // 設置可能な最大爆弾数
    float bombOffset_ = 2.0f;               // 爆弾設置位置のオフセット距離
    float bombScale_ = 1.0f;                // 爆弾のスケール
    float bpmProgress_ = 0.0f;              // BPMに同期した進行度（0.0～1.0）
    float bpmToleranceRange_ = 0.2f;        // BPM進行度の許容範囲
    
    int mapWidth_ = 13;                     // マップの幅
    int mapHeight_ = 13;                    // マップの高さ
    
    const Input* input_ = nullptr;          // 入力システムへのポインタ
    SceneBase* scene_ = nullptr;            // シーンへのポインタ
    ScreenBuffer* screenBuffer_ = nullptr;  // スクリーンバッファへのポインタ
    ShadowMapBuffer* shadowMapBuffer_ = nullptr; // シャドウマップバッファへのポインタ
    
    std::vector<Object3DBase*> bombObjects_; // 設置した爆弾のリスト
};

} // namespace KashipanEngine