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
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return 0;
	return board_[row][col];
}

void PuzzleBoard::SetPanel(int row, int col, int type) {
	if (row < 0 || row >= size_ || col < 0 || col >= size_) return;
	board_[row][col] = type;
}

bool PuzzleBoard::IsGarbage(int row, int col) const {
	return GetPanel(row, col) == kGarbageType;
}

int PuzzleBoard::CountGarbage() const {
	int count = 0;
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (board_[r][c] == kGarbageType) count++;
		}
	}
	return count;
}

int PuzzleBoard::PlaceGarbageRandom(int count) {
	// 通常パネルの位置リストを作成
	std::vector<std::pair<int, int>> normalCells;
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (board_[r][c] > 0) {
				normalCells.push_back({ r, c });
			}
		}
	}
	// シャッフル
	for (int i = static_cast<int>(normalCells.size()) - 1; i > 0; i--) {
		int j = KashipanEngine::GetRandomInt(0, i);
		std::swap(normalCells[i], normalCells[j]);
	}
	int placed = 0;
	for (int i = 0; i < count && i < static_cast<int>(normalCells.size()); i++) {
		auto [r, c] = normalCells[i];
		board_[r][c] = kGarbageType;
		placed++;
	}
	return placed;
}

bool PuzzleBoard::RemoveOneGarbageRandom() {
	std::vector<std::pair<int, int>> garbageCells;
	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (board_[r][c] == kGarbageType) {
				garbageCells.push_back({ r, c });
			}
		}
	}
	if (garbageCells.empty()) return false;
	int idx = KashipanEngine::GetRandomInt(0, static_cast<int>(garbageCells.size()) - 1);
	auto [r, c] = garbageCells[idx];
	board_[r][c] = GetNextPanelFromTable();
	return true;
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

	// 横方向（お邪魔パネルはマッチ対象外）
	for (int r = 0; r < size_; r++) {
		int count = 1;
		for (int c = 1; c < size_; c++) {
			if (board_[r][c] > 0 && board_[r][c] == board_[r][c - 1]) {
				count++;
			} else {
				if (count >= minCount && board_[r][c - 1] > 0) {
					segs.push_back({ true, r, c - count, count, board_[r][c - 1] });
				}
				count = 1;
			}
		}
		if (count >= minCount && board_[r][size_ - 1] > 0) {
			segs.push_back({ true, r, size_ - count, count, board_[r][size_ - 1] });
		}
	}

	// 縦方向
	for (int c = 0; c < size_; c++) {
		int count = 1;
		for (int r = 1; r < size_; r++) {
			if (board_[r][c] > 0 && board_[r][c] == board_[r - 1][c]) {
				count++;
			} else {
				if (count >= minCount && board_[r - 1][c] > 0) {
					segs.push_back({ false, c, r - count, count, board_[r - 1][c] });
				}
				count = 1;
			}
		}
		if (count >= minCount && board_[size_ - 1][c] > 0) {
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

			MatchResult mr;
			mr.type = MatchType::Square;
			mr.panelType = type;
			for (int dr = 0; dr < 3; dr++) {
				for (int dc = 0; dc < 3; dc++) {
					mr.cells.push_back({ r + dr, c + dc });
				}
			}
			results.push_back(std::move(mr));
		}
	}

	// --- 2. Cross ---
	for (int r = 1; r < size_ - 1; r++) {
		for (int c = 1; c < size_ - 1; c++) {
			int type = board_[r][c];
			if (type <= 0) continue;
			if (board_[r - 1][c] != type) continue;
			if (board_[r + 1][c] != type) continue;
			if (board_[r][c - 1] != type) continue;
			if (board_[r][c + 1] != type) continue;

			MatchResult mr;
			mr.type = MatchType::Cross;
			mr.panelType = type;
			mr.cells = {
				{r - 1, c}, {r, c - 1}, {r, c}, {r, c + 1}, {r + 1, c}
			};
			results.push_back(std::move(mr));
		}
	}

	// --- 3. Straight ---
	{
		auto segs = DetectLineMatches(straightMin);
		for (auto& seg : segs) {
			std::vector<std::pair<int, int>> cells;
			if (seg.isHorizontal) {
				for (int i = 0; i < seg.length; i++) {
					cells.push_back({ seg.fixedIndex, seg.start + i });
				}
			} else {
				for (int i = 0; i < seg.length; i++) {
					cells.push_back({ seg.start + i, seg.fixedIndex });
				}
			}

			MatchResult mr;
			mr.type = MatchType::Straight;
			mr.panelType = seg.type;
			mr.cells = std::move(cells);
			mr.isHorizontal = seg.isHorizontal;
			mr.fixedIndex = seg.fixedIndex;
			results.push_back(std::move(mr));
		}
	}

	// --- 4. Normal ---
	{
		auto segs = DetectLineMatches(normalMin);
		for (auto& seg : segs) {
			std::vector<std::pair<int, int>> cells;
			if (seg.isHorizontal) {
				for (int i = 0; i < seg.length; i++) {
					cells.push_back({ seg.fixedIndex, seg.start + i });
				}
			} else {
				for (int i = 0; i < seg.length; i++) {
					cells.push_back({ seg.start + i, seg.fixedIndex });
				}
			}

			MatchResult mr;
			mr.type = MatchType::Normal;
			mr.panelType = seg.type;
			mr.cells = std::move(cells);
			mr.isHorizontal = seg.isHorizontal;
			mr.fixedIndex = seg.fixedIndex;
			results.push_back(std::move(mr));
		}
	}

	// --- 5. 隣接お邪魔パネルを追加 ---
	// 消えるセルに隣接しているお邪魔パネルも一緒に消す
	std::set<std::pair<int, int>> matchedCells;
	for (auto& mr : results) {
		for (auto& cell : mr.cells) {
			matchedCells.insert(cell);
		}
	}

	std::set<std::pair<int, int>> garbageToRemove;
	static const int dr[] = { -1, 1, 0, 0 };
	static const int dc[] = { 0, 0, -1, 1 };
	for (auto& [r, c] : matchedCells) {
		for (int d = 0; d < 4; d++) {
			int nr = r + dr[d];
			int nc = c + dc[d];
			if (nr < 0 || nr >= size_ || nc < 0 || nc >= size_) continue;
			if (board_[nr][nc] == kGarbageType && matchedCells.find({ nr, nc }) == matchedCells.end()) {
				garbageToRemove.insert({ nr, nc });
			}
		}
	}
	// お邪魔パネルをダミーマッチとして追加
	if (!garbageToRemove.empty()) {
		MatchResult garbageMatch;
		garbageMatch.type = MatchType::Normal;
		garbageMatch.panelType = kGarbageType;
		for (auto& cell : garbageToRemove) {
			garbageMatch.cells.push_back(cell);
		}
		results.push_back(std::move(garbageMatch));
	}

	return results;
}

int PuzzleBoard::ClearAndFillMatches(const std::vector<MatchResult>& matches) {
	if (matches.empty()) return 0;

	std::vector<std::vector<bool>> cleared(size_, std::vector<bool>(size_, false));
	int totalCleared = 0;

	for (const auto& m : matches) {
		for (auto& [r, c] : m.cells) {
			if (!cleared[r][c]) {
				cleared[r][c] = true;
				totalCleared++;
			}
		}
	}

	for (int r = 0; r < size_; r++) {
		for (int c = 0; c < size_; c++) {
			if (!cleared[r][c]) continue;
			board_[r][c] = GetNextPanelFromTable();
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
