#include "Objects/Puzzle/PuzzleBoard.h"
#include <algorithm>

namespace Application {

void PuzzleBoard::Initialize(int size, int panelTypeCount) {
	size_ = size;
	panelTypeCount_ = panelTypeCount;
	board_.resize(size, std::vector<int>(size, 0));

	// ランダムにパネルを敷き詰める（1マスだけ空きを残す）
	// まず全マスにランダムなパネルを配置
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			board_[r][c] = KashipanEngine::GetRandomInt(1, panelTypeCount_);
		}
	}

	// ランダムな1マスを空きにする
	int emptyR = KashipanEngine::GetRandomInt(0, size_ - 1);
	int emptyC = KashipanEngine::GetRandomInt(0, size_ - 1);
	board_[emptyR][emptyC] = 0;
	emptyPos_ = { emptyR, emptyC };
}

int PuzzleBoard::GetPanel(int row, int col) const {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return -1;
	return board_[row][col];
}

void PuzzleBoard::SetPanel(int row, int col, int type) {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return;
	board_[row][col] = type;
	if (type == 0) {
		emptyPos_ = { row, col };
	}
}

bool PuzzleBoard::TryMovePanel(int row, int col) {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return false;
	if (board_[row][col] == 0) return false;

	auto [er, ec] = emptyPos_;
	int dr = er - row;
	int dc = ec - col;

	// 上下左右1マスの隣接チェック
	if ((std::abs(dr) == 1 && dc == 0) || (dr == 0 && std::abs(dc) == 1)) {
		// パネルを空きマスに移動
		board_[er][ec] = board_[row][col];
		board_[row][col] = 0;
		emptyPos_ = { row, col };
		return true;
	}

	return false;
}

bool PuzzleBoard::HasAdjacentEmpty(int row, int col) const {
	auto [er, ec] = emptyPos_;
	int dr = er - row;
	int dc = ec - col;
	return (std::abs(dr) == 1 && dc == 0) || (dr == 0 && std::abs(dc) == 1);
}

std::vector<std::vector<int>> PuzzleBoard::GetCenter3x3() const {
	int center = size_ / 2;
	std::vector<std::vector<int>> result(3, std::vector<int>(3, 0));
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			int br = center - 1 + r;
			int bc = center - 1 + c;
			if (br >= 0 && br < size_ && bc >= 0 && bc < size_) {
				result[r][c] = board_[br][bc];
			}
		}
	}
	return result;
}

void PuzzleBoard::SetBoard(const std::vector<std::vector<int>>& board) {
	board_ = board;
	size_ = static_cast<int>(board.size());
	// 空きマスの位置を探す
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (board_[r][c] == 0) {
				emptyPos_ = { r, c };
				return;
			}
		}
	}
}

} // namespace Application
