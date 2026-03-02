#include "Objects/Puzzle/PuzzleGoal.h"
#include "Objects/Puzzle/PuzzleBoard.h"
#include <algorithm>

namespace Application {

void PuzzleGoal::Generate(const PuzzleBoard& board, [[maybe_unused]] int panelTypeCount, int moveCount) {
	// 現在のボード状態をコピーして、n回ランダムに移動した結果の中央3x3を目標とする
	// これにより必ずn回の移動で作成可能であることが保証される
	// ただし目標パターンの中央3x3に空きマスが含まれないようにする

	int size = board.GetSize();
	constexpr int kMaxAttempts = 100;

	for (int attempt = 0; attempt < kMaxAttempts; attempt++) {
		PuzzleBoard tempBoard;
		tempBoard.SetBoard(board.GetBoard());

		// moveCount回ランダムに移動
		int moved = 0;
		int lastMoveDir = -1;
		while (moved < moveCount) {
			auto [er, ec] = tempBoard.GetEmptyPos();

			struct Move { int row; int col; int dir; };
			std::vector<Move> candidates;
			if (er > 0) candidates.push_back({ er - 1, ec, 0 });
			if (er < size - 1) candidates.push_back({ er + 1, ec, 1 });
			if (ec > 0) candidates.push_back({ er, ec - 1, 2 });
			if (ec < size - 1) candidates.push_back({ er, ec + 1, 3 });

			int oppositeDir = -1;
			if (lastMoveDir == 0) oppositeDir = 1;
			else if (lastMoveDir == 1) oppositeDir = 0;
			else if (lastMoveDir == 2) oppositeDir = 3;
			else if (lastMoveDir == 3) oppositeDir = 2;

			if (oppositeDir >= 0) {
				candidates.erase(
					std::remove_if(candidates.begin(), candidates.end(),
						[oppositeDir](const Move& m) { return m.dir == oppositeDir; }),
					candidates.end());
			}

			if (candidates.empty()) break;

			int idx = KashipanEngine::GetRandomInt(0, static_cast<int>(candidates.size()) - 1);
			auto& chosen = candidates[idx];
			tempBoard.TryMovePanel(chosen.row, chosen.col);
			lastMoveDir = chosen.dir;
			moved++;
		}

		// 移動後の中央3x3を目標パターン候補とする
		auto candidate = tempBoard.GetCenter3x3();

		// 中央3x3に空きマスが含まれていないか確認
		bool hasEmpty = false;
		for (int r = 0; r < 3 && !hasEmpty; r++) {
			for (int c = 0; c < 3 && !hasEmpty; c++) {
				if (candidate[r][c] == 0) hasEmpty = true;
			}
		}

		// 現在のボードの中央3x3と異なるか確認
		bool isDifferent = false;
		auto currentCenter = board.GetCenter3x3();
		for (int r = 0; r < 3 && !isDifferent; r++) {
			for (int c = 0; c < 3 && !isDifferent; c++) {
				if (candidate[r][c] != currentCenter[r][c]) isDifferent = true;
			}
		}

		if (!hasEmpty && isDifferent) {
			goalPattern_ = candidate;
			return;
		}
	}

	// フォールバック：最後の試行結果をそのまま使う
	PuzzleBoard tempBoard;
	tempBoard.SetBoard(board.GetBoard());
	int lastMoveDir = -1;
	for (int i = 0; i < moveCount; i++) {
		auto [er, ec] = tempBoard.GetEmptyPos();
		struct Move { int row; int col; int dir; };
		std::vector<Move> candidates;
		if (er > 0) candidates.push_back({ er - 1, ec, 0 });
		if (er < size - 1) candidates.push_back({ er + 1, ec, 1 });
		if (ec > 0) candidates.push_back({ er, ec - 1, 2 });
		if (ec < size - 1) candidates.push_back({ er, ec + 1, 3 });

		int oppositeDir = -1;
		if (lastMoveDir == 0) oppositeDir = 1;
		else if (lastMoveDir == 1) oppositeDir = 0;
		else if (lastMoveDir == 2) oppositeDir = 3;
		else if (lastMoveDir == 3) oppositeDir = 2;
		if (oppositeDir >= 0) {
			candidates.erase(
				std::remove_if(candidates.begin(), candidates.end(),
					[oppositeDir](const Move& m) { return m.dir == oppositeDir; }),
				candidates.end());
		}
		if (candidates.empty()) break;
		int idx = KashipanEngine::GetRandomInt(0, static_cast<int>(candidates.size()) - 1);
		tempBoard.TryMovePanel(candidates[idx].row, candidates[idx].col);
		lastMoveDir = candidates[idx].dir;
	}
	goalPattern_ = tempBoard.GetCenter3x3();
}

bool PuzzleGoal::IsGoalReached(const PuzzleBoard& board) const {
	auto center = board.GetCenter3x3();
	if (goalPattern_.size() != 3 || center.size() != 3) return false;
	for (int r = 0; r < 3; r++) {
		for (int c = 0; c < 3; c++) {
			// 目標パターンが0（空き）のマスは比較しない
			if (goalPattern_[r][c] == 0) continue;
			if (goalPattern_[r][c] != center[r][c]) return false;
		}
	}
	return true;
}

} // namespace Application
