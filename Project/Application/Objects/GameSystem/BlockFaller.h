#pragma once
#include <vector>
#include <stdint.h>

#include <Objects/Container/BlockContainer.h>

namespace Application
{
    /// ブロックが下に落ちる挙動を管理するクラス
    class BlockFaller
    {
    public:
        /// 落下速度と落下幅のオフセットを指定して初期化
        void Initialize(float fallSpeed, float fallOffset);
        
        /// 落下を更新する。
        void Update(float deltaTime, BlockContainer& container);

        /// 現在落下中かどうかを返す。
        bool IsFalling() const { return isFalling_; }
        
        /// 落下（1マス分）が完了したフレームかどうかを返す。Update()を呼び出した後に呼び出すこと。
        bool IsFallComplete() const { return isFallCompleteFrame_; }
        
        /// 現在の落下オフセットを取得する（0.0〜fallOffset_）。描画時に使用する。
        float GetCurrentFallOffset() const { return currentFallDelta_; }

        /// 現在落下しているブロックの座標リストを返す。
        const std::vector<std::pair<int32_t, int32_t>>& GetFallingBlocks() const { return fallingBlocks_; }

    private:
        float fallSpeed_; // 落下速度 (毎秒)
        float fallOffset_; // 落下幅 (1ブロックのサイズ)
        float currentFallDelta_; // 現在の落下量 (0〜fallOffset_)
        bool isFalling_; // 落下中フラグ
        bool isFallCompleteFrame_; // 落下完了フレームフラグ
        
        // 落下対象のブロック座標 (row, col)
        std::vector<std::pair<int32_t, int32_t>> fallingBlocks_;
    };
}
