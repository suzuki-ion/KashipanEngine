#include "BlockFaller.h"

namespace Application
{
    void BlockFaller::Initialize(float fallSpeed, float fallOffset)
    {
        fallSpeed_ = fallSpeed;
        fallOffset_ = fallOffset;
        currentFallDelta_ = 0.0f;
        isFalling_ = false;
        isFallCompleteFrame_ = false;
        fallingBlocks_.clear();
    }

    void BlockFaller::Update(float deltaTime, BlockContainer& container)
    {
        isFallCompleteFrame_ = false;

        if (!isFalling_)
        {
            // 落下が必要なブロックがあるかチェック
            fallingBlocks_.clear();
            int rows = container.GetRows();
            int cols = container.GetCols();

            // 下から上にチェック (row 0は最下層)
            // row 0 は下に落ちる場所がないのでスキップ
            for (int row = 1; row < rows; ++row)
            {
                for (int col = 0; col < cols; ++col)
                {
                    int32_t currentBlock = container.GetBlock(row, col);
                    int32_t belowBlock = container.GetBlock(row - 1, col);

                    // 自分がブロックで、下が空白(0)なら落下対象
                    if (currentBlock != 0 && belowBlock == 0)
                    {
                        fallingBlocks_.push_back({ row, col });
                    }
                }
            }

            if (!fallingBlocks_.empty())
            {
                isFalling_ = true;
                currentFallDelta_ = 0.0f;
            }
        }

        if (isFalling_)
        {
            currentFallDelta_ += fallSpeed_ * deltaTime;
            if (currentFallDelta_ >= fallOffset_)
            {
                currentFallDelta_ = 0.0f;
                isFalling_ = false;
                isFallCompleteFrame_ = true;

                // コンテナのデータを実際に更新する
                // 落下完了したブロックを1つ下に移動
                // 注意: 複数の列で落下が発生している可能性がある。
                // 重複を避けるため、一旦コピーを持ってから更新するのが安全
                struct MoveInfo { int r, c, val; };
                std::vector<MoveInfo> moves;
                for (auto& pos : fallingBlocks_)
                {
                    moves.push_back({ pos.first, pos.second, container.GetBlock(pos.first, pos.second) });
                }

                // 元の位置を消去
                for (auto& m : moves)
                {
                    container.SetBlock(m.r, m.c, 0);
                }
                // 下の位置に設定
                for (auto& m : moves)
                {
                    container.SetBlock(m.r - 1, m.c, m.val);
                }
                
                fallingBlocks_.clear();
            }
        }
    }
}
