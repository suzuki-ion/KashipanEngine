#include "Objects/Puzzle/PuzzleBoard.h"
#include <algorithm>
#include <numeric>
#include <set>

namespace Application {

void PuzzleBoard::Initialize(int size) {
	size_ = size;
	board_.resize(size, std::vector<int>(size, 0));
	spawnTable_.clear();

	// ランダムにパズルパネルを敷き詰める（出現テーブルを使用）
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			board_[r][c] = GetNextPanelFromTable();
		}
	}
}

int PuzzleBoard::GetPanel(int row, int col) const {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return -1;
	return board_[row][col];
}

void PuzzleBoard::SetPanel(int row, int col, int type) {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return;
	board_[row][col] = type;
}

void PuzzleBoard::ShiftRowRight(int row) {
	if (row < 0 || row >= size_ || size_ <= 1) return;
	int last = board_[row][size_ - 1];
	for (int c = size_ - 1; c > 0; c--) {
		board_[row][c] = board_[row][c - 1];
	}
	board_[row][0] = last;
}

void PuzzleBoard::ShiftRowLeft(int row) {
	if (row < 0 || row >= size_ || size_ <= 1) return;
	int first = board_[row][0];
	for (int c = 0; c < size_ - 1; c++) {
		board_[row][c] = board_[row][c + 1];
	}
	board_[row][size_ - 1] = first;
}

void PuzzleBoard::ShiftColUp(int col) {
	if (col < 0 || col >= size_ || size_ <= 1) return;
	int last = board_[size_ - 1][col];
	for (int r = size_ - 1; r > 0; r--) {
		board_[r][col] = board_[r - 1][col];
	}
	board_[0][col] = last;
}

void PuzzleBoard::ShiftColDown(int col) {
	if (col < 0 || col >= size_ || size_ <= 1) return;
	int first = board_[0][col];
	for (int r = 0; r < size_ - 1; r++) {
		board_[r][col] = board_[r + 1][col];
	}
	board_[size_ - 1][col] = first;
}

std::vector<PuzzleBoard::MatchLine> PuzzleBoard::DetectMatches(int minMatch) const {
	std::vector<MatchLine> matches;

	// 横方向のマッチ
	for (int r = 0; r < size_; r++) {
		int count = 1;
		for (int c = 1; c < size_; c++) {
			if (board_[r][c] == board_[r][c - 1] && board_[r][c] > 0) {
				count++;
			} else {
				if (count >= minMatch) {
					matches.push_back({ true, r, c - count, count, board_[r][c - 1] });
				}
				count = 1;
			}
		}
		if (count >= minMatch) {
			matches.push_back({ true, r, size_ - count, count, board_[r][size_ - 1] });
		}
	}

	// 縦方向のマッチ
	for (int c = 0; c < size_; c++) {
		int count = 1;
		for (int r = 1; r < size_; r++) {
			if (board_[r][c] == board_[r - 1][c] && board_[r][c] > 0) {
				count++;
			} else {
				if (count >= minMatch) {
					matches.push_back({ false, c, r - count, count, board_[r - 1][c] });
				}
				count = 1;
			}
		}
		if (count >= minMatch) {
			matches.push_back({ false, c, size_ - count, count, board_[size_ - 1][c] });
		}
	}

	return matches;
}

int PuzzleBoard::ClearAndFillMatches(const std::vector<MatchLine>& matches) {
	if (matches.empty()) return 0;

	// マッチした位置をマーク
	std::vector<std::vector<bool>> cleared(size_, std::vector<bool>(size_, false));
	int totalCleared = 0;

	for (const auto& m : matches) {
		if (m.isHorizontal) {
			// 横マッチ
			for (int i = 0; i < m.length; i++) {
				int c = m.start + i;
				if (!cleared[m.fixedIndex][c]) {
					cleared[m.fixedIndex][c] = true;
					totalCleared++;
				}
			}
		} else {
			// 縦マッチ
			for (int i = 0; i < m.length; i++) {
				int r = m.start + i;
				if (!cleared[r][m.fixedIndex]) {
					cleared[r][m.fixedIndex] = true;
					totalCleared++;
				}
			}
		}
	}

	// 横マッチの処理: 消えた行について、消えた分だけ左のパネルが右に移動し、左端から新パネル
	// 行ごとに処理
	std::set<int> hRows;
	for (const auto& m : matches) {
		if (m.isHorizontal) {
			hRows.insert(m.fixedIndex);
		}
	}
	for (int row : hRows) {
		// この行で消えたマスを処理
		std::vector<int> remaining;
		for (int c = 0; c < size_; c++) {
			if (!cleared[row][c]) {
				remaining.push_back(board_[row][c]);
			}
		}
		int remainCount = static_cast<int>(remaining.size());
		int fillCount = size_ - remainCount;
		// 新パネルを左から、残存パネルを右に配置
		for (int c = 0; c < fillCount; c++) {
			board_[row][c] = GetNextPanelFromTable();
		}
		for (int c = 0; c < remainCount; c++) {
			board_[row][fillCount + c] = remaining[c];
		}
		// この行のclearedフラグをリセット（二重処理防止）
		for (int c = 0; c < size_; c++) {
			cleared[row][c] = false;
		}
	}

	// 縦マッチの処理: 消えた列について、消えた分だけ上(row+方向)のパネルが下に移動し、上端から新パネル
	std::set<int> vCols;
	for (const auto& m : matches) {
		if (!m.isHorizontal) {
			vCols.insert(m.fixedIndex);
		}
	}
	for (int col : vCols) {
		// この列で消えたマスを処理
		std::vector<int> remaining;
		for (int r = 0; r < size_; r++) {
			if (!cleared[r][col]) {
				remaining.push_back(board_[r][col]);
			}
		}
		int remainCount = static_cast<int>(remaining.size());
		// 残存パネルを下から、新パネルを上端に配置
		for (int r = 0; r < remainCount; r++) {
			board_[r][col] = remaining[r];
		}
		for (int r = remainCount; r < size_; r++) {
			board_[r][col] = GetNextPanelFromTable();
		}
	}

	return totalCleared;
}

void PuzzleBoard::SetBoard(const std::vector<std::vector<int>>& board) {
	board_ = board;
	size_ = static_cast<int>(board.size());
}

int PuzzleBoard::GetNextPanelFromTable() {
	if (spawnTable_.empty()) {
		RefillSpawnTable();
	}
	int type = spawnTable_.front();
	spawnTable_.pop_front();
	return type;
}

void PuzzleBoard::RefillSpawnTable() {
	// 存在する種類分（1～size_）のテーブルを用意
	std::vector<int> table(size_);
	std::iota(table.begin(), table.end(), 1); // 1, 2, ..., size_
	// ランダムにシャッフル
	for (int i = static_cast<int>(table.size()) - 1; i > 0; i--) {
		int j = KashipanEngine::GetRandomInt(0, i);
		std::swap(table[i], table[j]);
	}
	for (int t : table) {
		spawnTable_.push_back(t);
	}
}

} // namespace Application
