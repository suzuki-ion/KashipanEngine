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
}

void PuzzleCursor::Update(KashipanEngine::InputCommand* inputCommand, float deltaTime) {
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
		return; // イージング中は入力を受け付けない
	}

	if (!inputCommand) return;

	int newRow = row_;
	int newCol = col_;

	// W / ↑ / 左スティックY+ / 十字キー上 → 上（row増加=Z+方向=画面奥）
	if (inputCommand->Evaluate("PuzzleUp").Triggered()) {
		newRow++;
	}
	// S / ↓ / 左スティックY- / 十字キー下 → 下（row減少=Z-方向=画面手前）
	if (inputCommand->Evaluate("PuzzleDown").Triggered()) {
		newRow--;
	}
	// A / ← / 左スティックX- / 十字キー左 → 左
	if (inputCommand->Evaluate("PuzzleLeft").Triggered()) {
		newCol--;
	}
	// D / → / 左スティックX+ / 十字キー右 → 右
	if (inputCommand->Evaluate("PuzzleRight").Triggered()) {
		newCol++;
	}

	// 範囲制限
	newRow = std::clamp(newRow, 0, boardSize_ - 1);
	newCol = std::clamp(newCol, 0, boardSize_ - 1);

	if (newRow != row_ || newCol != col_) {
		// イージング開始
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

} // namespace Application
