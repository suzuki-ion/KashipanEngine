#include "Objects/Puzzle/PuzzleCursor.h"
#include <algorithm>

namespace Application {

void PuzzleCursor::Initialize(int startRow, int startCol, int boardSize, float easingDuration) {
	row_ = startRow;
	col_ = startCol;
	boardSize_ = boardSize;
	easingDuration_ = easingDuration;
	currentRow_ = static_cast<float>(startRow);
	currentCol_ = static_cast<float>(startCol);
	isMoving_ = false;
	moveTimer_ = 0.0f;
	hasMoveAction_ = false;
	moveActionDir_ = -1;
}

void PuzzleCursor::Update(KashipanEngine::InputCommand* inputCommand, float deltaTime, bool panelsAnimating) {
	hasMoveAction_ = false;
	moveActionDir_ = -1;

	// イージング更新
	if (isMoving_) {
		moveTimer_ += deltaTime;
		float t = std::clamp(moveTimer_ / easingDuration_, 0.0f, 1.0f);
		float easedT = Apply(t, EaseType::EaseOutCubic);
		currentRow_ = Lerp(startRow_, targetRow_, easedT);
		currentCol_ = Lerp(startCol_, targetCol_, easedT);

		if (t >= 1.0f) {
			isMoving_ = false;
			currentRow_ = targetRow_;
			currentCol_ = targetCol_;
		}
		return;
	}

	if (!inputCommand) return;

	// Spaceキー/Aボタンが押されている場合 → 移動アクション
	bool actionHeld = inputCommand->Evaluate("PuzzleActionHold").Triggered();

	// 方向入力
	int dirUp = inputCommand->Evaluate("PuzzleUp").Triggered() ? 1 : 0;
	int dirDown = inputCommand->Evaluate("PuzzleDown").Triggered() ? 1 : 0;
	int dirLeft = inputCommand->Evaluate("PuzzleLeft").Triggered() ? 1 : 0;
	int dirRight = inputCommand->Evaluate("PuzzleRight").Triggered() ? 1 : 0;

	if (actionHeld && !panelsAnimating) {
		// 移動アクション: 行/列のパネルを動かす
		if (dirUp) {
			hasMoveAction_ = true;
			moveActionDir_ = 0; // 上
		} else if (dirDown) {
			hasMoveAction_ = true;
			moveActionDir_ = 1; // 下
		} else if (dirLeft) {
			hasMoveAction_ = true;
			moveActionDir_ = 2; // 左
		} else if (dirRight) {
			hasMoveAction_ = true;
			moveActionDir_ = 3; // 右
		}
	} else if (!actionHeld) {
		// カーソル移動
		int newRow = row_;
		int newCol = col_;

		if (dirUp) newRow++;
		if (dirDown) newRow--;
		if (dirLeft) newCol--;
		if (dirRight) newCol++;

		newRow = std::clamp(newRow, 0, boardSize_ - 1);
		newCol = std::clamp(newCol, 0, boardSize_ - 1);

		if (newRow != row_ || newCol != col_) {
			startRow_ = currentRow_;
			startCol_ = currentCol_;
			targetRow_ = static_cast<float>(newRow);
			targetCol_ = static_cast<float>(newCol);
			row_ = newRow;
			col_ = newCol;
			isMoving_ = true;
			moveTimer_ = 0.0f;
		}
	}
}

} // namespace Application
