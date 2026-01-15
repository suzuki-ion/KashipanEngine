#pragma once

#include "KashipanEngine.h"
#include "../Objects/Components/Player/BpmbSpawn.h"
#include <vector>
#include <memory>

namespace KashipanEngine {

/// @brief 爆弾オブジェクトの管理を行うシステム
/// @note Scene内で爆弾の生成、削除、状態管理を一元的に行う
class BombManager {
public:
    /// @brief コンストラクタ
    BombManager() = default;
    ~BombManager() = default;

    /// @brief 更新処理
    /// @param input 入力システム
    void Update(const Input* input);

    /// @brief 爆弾設置リクエストを処理
    /// @param request 爆弾設置リクエスト
    /// @return 設置に成功した場合はtrue
    bool ProcessPlacementRequest(const BombPlacementRequest& request);

    /// @brief 指定したマップ座標に既に爆弾が存在するかチェック
    /// @param mapX マップX座標
    /// @param mapZ マップZ座標
    /// @return 爆弾が存在する場合はtrue
    bool IsBombAtPosition(int mapX, int mapZ) const;

    /// @brief 爆弾を削除（爆発時などに呼ばれる想定）
    /// @param index 削除する爆弾のインデックス
    void RemoveBomb(size_t index);

    /// @brief すべての爆弾をクリア
    void ClearAllBombs();

    /// @brief 爆弾の数を取得
    size_t GetBombCount() const { return bombs_.size(); }

    /// @brief 爆弾オブジェクトのリストを取得
    const std::vector<std::unique_ptr<Object3DBase>>& GetBombs() const { return bombs_; }

    /// @brief 爆弾オブジェクトを移動して取得（Scene追加用）
    /// @param index 取得する爆弾のインデックス
    /// @return 爆弾オブジェクト
    std::unique_ptr<Object3DBase> TakeBomb(size_t index);

    /// @brief レンダリング設定
    void SetRenderBuffers(ScreenBuffer* screenBuffer, ShadowMapBuffer* shadowMapBuffer) {
        screenBuffer_ = screenBuffer;
        shadowMapBuffer_ = shadowMapBuffer;
    }

    /// @brief BPM進行度を設定
    void SetBPMProgress(float bpmProgress) { bpmProgress_ = bpmProgress; }

    /// @brief 登録されているBombSpawnコンポーネントを設定
    void RegisterBombSpawn(BombSpawn* bombSpawn);

    /// @brief 登録されているBombSpawnコンポーネントを解除
    void UnregisterBombSpawn(BombSpawn* bombSpawn);

private:
    /// @brief 爆弾オブジェクトを作成
    /// @param request 爆弾設置リクエスト
    /// @return 作成された爆弾オブジェクト
    std::unique_ptr<Object3DBase> CreateBombObject(const BombPlacementRequest& request);

    /// @brief ワールド座標からマップ座標を取得
    /// @param worldPos ワールド座標
    /// @param outMapX マップX座標の出力先
    /// @param outMapZ マップZ座標の出力先
    void WorldToMapCoordinates(const Vector3& worldPos, int& outMapX, int& outMapZ) const;

    std::vector<std::unique_ptr<Object3DBase>> bombs_;  ///< 管理している爆弾オブジェクト
    std::vector<BombSpawn*> registeredSpawners_;        ///< 登録されているBombSpawnコンポーネント
    
    ScreenBuffer* screenBuffer_ = nullptr;              ///< スクリーンバッファ
    ShadowMapBuffer* shadowMapBuffer_ = nullptr;        ///< シャドウマップバッファ
    
    float bpmProgress_ = 0.0f;                          ///< BPM進行度
};

} // namespace KashipanEngine
