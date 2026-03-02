#pragma once
#include <KashipanEngine.h>
#include <vector>
#include <utility>

namespace Application {

/// パズルボードのデータ管理クラス
/// 二次元配列でパネルの種類を管理する。0は空きマスを表す。
class PuzzleBoard {
public:
	void Initialize(int size, int panelTypeCount);

	/// ボードのサイズ（n x n）
	int GetSize() const { return size_; }

	/// 指定位置のパネル種類を取得（0=空き）
	int GetPanel(int row, int col) const;

	/// 指定位置のパネル種類を設定
	void SetPanel(int row, int col, int type);

	/// 空きマスの位置を取得
	std::pair<int, int> GetEmptyPos() const { return emptyPos_; }

	/// 指定位置のパネルを空きマス方向に移動する
	/// 移動が成功した場合trueを返す
	bool TryMovePanel(int row, int col);

	/// 指定位置の上下左右に空きマスがあるかチェック
	bool HasAdjacentEmpty(int row, int col) const;

	/// ボードの中央3x3の状態を取得
	std::vector<std::vector<int>> GetCenter3x3() const;

	/// ボード全体の状態を取得
	const std::vector<std::vector<int>>& GetBoard() const { return board_; }

	/// ボードの状態を設定
	void SetBoard(const std::vector<std::vector<int>>& board);

private:
	int size_ = 0;
	int panelTypeCount_ = 0;
	std::vector<std::vector<int>> board_;
	std::pair<int, int> emptyPos_ = { 0, 0 };
};

} // namespace Application
