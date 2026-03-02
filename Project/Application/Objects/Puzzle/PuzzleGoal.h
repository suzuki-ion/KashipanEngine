#pragma once
#include <KashipanEngine.h>
#include <vector>

namespace Application {

class PuzzleBoard;

/// 目標パネルの管理クラス
/// 3x3の目標パターンを保持し、ステージ中央と比較する
class PuzzleGoal {
public:
	/// 目標パターンを生成する
	/// boardの現在の状態から、moveCount回の移動で到達可能なパターンをランダムに生成する
	void Generate(const PuzzleBoard& board, int panelTypeCount, int moveCount = 5);

	/// 目標パターンの取得（3x3）
	const std::vector<std::vector<int>>& GetGoalPattern() const { return goalPattern_; }

	/// ステージ中央3x3が目標パターンと一致しているかチェック
	bool IsGoalReached(const PuzzleBoard& board) const;

private:
	std::vector<std::vector<int>> goalPattern_;
};

} // namespace Application
