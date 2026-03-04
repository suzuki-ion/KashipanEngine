#include "Objects/Puzzle/PuzzleBoard.h"
#include <algorithm>
#include <numeric>
#include <set>

namespace Application {

void PuzzleBoard::Initialize(int size, int panelTypeCount) {
	size_ = size;
	panelTypeCount_ = panelTypeCount;
	board_.resize(size, std::vector<int>(size, 0));
	spawnTable_.clear();

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

// ================================================================
// 直線マッチ検出ヘルパー
// ================================================================

std::vector<PuzzleBoard::LineSeg> PuzzleBoard::DetectLineMatches(int minCount) const {
	std::vector<LineSeg> segs;

	// 横方向
	for (int r = 0; r < size_; r++) {
		int count = 1;
		for (int c = 1; c < size_; c++) {
			if (board_[r][c] == board_[r][c - 1] && board_[r][c] > 0) {
				count++;
			} else {
				if (count >= minCount) {
					segs.push_back({ true, r, c - count, count, board_[r][c - 1] });
				}
				count = 1;
			}
		}
		if (count >= minCount) {
			segs.push_back({ true, r, size_ - count, count, board_[r][size_ - 1] });
		}
	}

	// 縦方向
	for (int c = 0; c < size_; c++) {
		int count = 1;
		for (int r = 1; r < size_; r++) {
			if (board_[r][c] == board_[r - 1][c] && board_[r][c] > 0) {
				count++;
			} else {
				if (count >= minCount) {
					segs.push_back({ false, c, r - count, count, board_[r - 1][c] });
				}
				count = 1;
			}
		}
		if (count >= minCount) {
			segs.push_back({ false, c, size_ - count, count, board_[size_ - 1][c] });
		}
	}

	return segs;
}

// ================================================================
// 全マッチパターン検出
// ================================================================

std::vector<PuzzleBoard::MatchResult> PuzzleBoard::DetectAllMatches(int normalMin, int straightMin) const {
	std::vector<MatchResult> results;
	// consumed: 既に上位パターンで消費済みのセル
	std::vector<std::vector<bool>> consumed(size_, std::vector<bool>(size_, false));

	// --- 1. Square (3x3) ---
	for (int r = 0; r <= size_ - 3; r++) {
		for (int c = 0; c <= size_ - 3; c++) {
			int type = board_[r][c];
			if (type <= 0) continue;
			bool allSame = true;
			for (int dr = 0; dr < 3 && allSame; dr++) {
				for (int dc = 0; dc < 3 && allSame; dc++) {
					if (board_[r + dr][c + dc] != type) allSame = false;
				}
			}
			if (!allSame) continue;
			// 消費済みでないか確認
			bool anyConsumed = false;
			for (int dr = 0; dr < 3 && !anyConsumed; dr++) {
				for (int dc = 0; dc < 3 && !anyConsumed; dc++) {
					if (consumed[r + dr][c + dc]) anyConsumed = true;
				}
			}
			if (anyConsumed) continue;

			MatchResult mr;
			mr.type = MatchType::Square;
			mr.panelType = type;
			for (int dr = 0; dr < 3; dr++) {
				for (int dc = 0; dc < 3; dc++) {
					mr.cells.push_back({ r + dr, c + dc });
					consumed[r + dr][c + dc] = true;
				}
			}
			results.push_back(std::move(mr));
		}
	}

	// --- 2. Cross (縦3+横3の十字) ---
	// 中心セルから縦3(中心±1)と横3(中心±1)が全て同色
	for (int r = 1; r < size_ - 1; r++) {
		for (int c = 1; c < size_ - 1; c++) {
			int type = board_[r][c];
			if (type <= 0) continue;
			// 十字: (r-1,c), (r,c-1), (r,c), (r,c+1), (r+1,c)
			if (board_[r - 1][c] != type) continue;
			if (board_[r + 1][c] != type) continue;
			if (board_[r][c - 1] != type) continue;
			if (board_[r][c + 1] != type) continue;

			std::vector<std::pair<int, int>> cells = {
				{r - 1, c}, {r, c - 1}, {r, c}, {r, c + 1}, {r + 1, c}
			};

			bool anyConsumed = false;
			for (auto& [cr, cc] : cells) {
				if (consumed[cr][cc]) { anyConsumed = true; break; }
			}
			if (anyConsumed) continue;

			MatchResult mr;
			mr.type = MatchType::Cross;
			mr.panelType = type;
			mr.cells = std::move(cells);
			for (auto& [cr, cc] : mr.cells) {
				consumed[cr][cc] = true;
			}
			results.push_back(std::move(mr));
		}
	}

	// --- 3. Straight (直線 straightMin 以上) ---
	{
		auto segs = DetectLineMatches(straightMin);
		for (auto& seg : segs) {
			std::vector<std::pair<int, int>> cells;
			bool anyConsumed = false;
			if (seg.isHorizontal) {
				for (int i = 0; i < seg.length; i++) {
					if (consumed[seg.fixedIndex][seg.start + i]) { anyConsumed = true; break; }
					cells.push_back({ seg.fixedIndex, seg.start + i });
				}
			} else {
				for (int i = 0; i < seg.length; i++) {
					if (consumed[seg.start + i][seg.fixedIndex]) { anyConsumed = true; break; }
					cells.push_back({ seg.start + i, seg.fixedIndex });
				}
			}
			if (anyConsumed) continue;

			MatchResult mr;
			mr.type = MatchType::Straight;
			mr.panelType = seg.type;
			mr.cells = std::move(cells);
			mr.isHorizontal = seg.isHorizontal;
			mr.fixedIndex = seg.fixedIndex;
			for (auto& [cr, cc] : mr.cells) {
				consumed[cr][cc] = true;
			}
			results.push_back(std::move(mr));
		}
	}

	// --- 4. Normal (直線 normalMin 以上) ---
	{
		auto segs = DetectLineMatches(normalMin);
		for (auto& seg : segs) {
			std::vector<std::pair<int, int>> cells;
			bool anyConsumed = false;
			if (seg.isHorizontal) {
				for (int i = 0; i < seg.length; i++) {
					if (consumed[seg.fixedIndex][seg.start + i]) { anyConsumed = true; break; }
					cells.push_back({ seg.fixedIndex, seg.start + i });
				}
			} else {
				for (int i = 0; i < seg.length; i++) {
					if (consumed[seg.start + i][seg.fixedIndex]) { anyConsumed = true; break; }
					cells.push_back({ seg.start + i, seg.fixedIndex });
				}
			}
			if (anyConsumed) continue;

			MatchResult mr;
			mr.type = MatchType::Normal;
			mr.panelType = seg.type;
			mr.cells = std::move(cells);
			mr.isHorizontal = seg.isHorizontal;
			mr.fixedIndex = seg.fixedIndex;
			for (auto& [cr, cc] : mr.cells) {
				consumed[cr][cc] = true;
			}
			results.push_back(std::move(mr));
		}
	}

	return results;
}

int PuzzleBoard::ClearAndFillMatches(const std::vector<MatchResult>& matches) {
	if (matches.empty()) return 0;

	// マッチした位置をマーク
	std::vector<std::vector<bool>> cleared(size_, std::vector<bool>(size_, false));
	int totalCleared = 0;

	// 横消し行と縦消し列を追跡
	std::set<int> hRows;
	std::set<int> vCols;

	for (const auto& m : matches) {
		for (auto& [r, c] : m.cells) {
			if (!cleared[r][c]) {
				cleared[r][c] = true;
				totalCleared++;
			}
		}
		// 方向の追跡（Normal/Straightは方向あり、Cross/Squareは両方向扱い）
		if (m.type == MatchType::Normal || m.type == MatchType::Straight) {
			if (m.isHorizontal) {
				hRows.insert(m.fixedIndex);
			} else {
				vCols.insert(m.fixedIndex);
			}
		} else {
			// Cross/Square: 含まれる行と列を全て対象
			for (auto& [r, c] : m.cells) {
				// 行に消えたセルがあれば横方向補充、列にあれば縦方向補充
				// 簡易的に：消えたセルの行は横扱い
				hRows.insert(r);
			}
		}
	}

	// 横マッチ行の処理
	for (int row : hRows) {
		std::vector<int> remaining;
		for (int c = 0; c < size_; c++) {
			if (!cleared[row][c]) {
				remaining.push_back(board_[row][c]);
			}
		}
		int remainCount = static_cast<int>(remaining.size());
		int fillCount = size_ - remainCount;
		for (int c = 0; c < fillCount; c++) {
			board_[row][c] = GetNextPanelFromTable();
		}
		for (int c = 0; c < remainCount; c++) {
			board_[row][fillCount + c] = remaining[c];
		}
		for (int c = 0; c < size_; c++) {
			cleared[row][c] = false;
		}
	}

	// 縦マッチ列の処理
	for (int col : vCols) {
		std::vector<int> remaining;
		for (int r = 0; r < size_; r++) {
			if (!cleared[r][col]) {
				remaining.push_back(board_[r][col]);
			}
		}
		int remainCount = static_cast<int>(remaining.size());
		for (int r = 0; r < remainCount; r++) {
			board_[r][col] = remaining[r];
		}
		for (int r = remainCount; r < size_; r++) {
			board_[r][col] = GetNextPanelFromTable();
		}
	}

	return totalCleared;
}

bool PuzzleBoard::IsEmpty() const {
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (board_[r][c] > 0) return false;
		}
	}
	return true;
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
	int count = (panelTypeCount_ > 0) ? panelTypeCount_ : size_;
	std::vector<int> table(count);
	std::iota(table.begin(), table.end(), 1);
	for (int i = static_cast<int>(table.size()) - 1; i > 0; i--) {
		int j = KashipanEngine::GetRandomInt(0, i);
		std::swap(table[i], table[j]);
	}
	for (int t : table) {
		spawnTable_.push_back(t);
	}
}

} // namespace Application
