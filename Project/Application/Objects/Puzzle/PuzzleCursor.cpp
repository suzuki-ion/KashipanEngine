#include "Objects/Puzzle/PuzzleCursor.h"
#include <algorithm>

namespace Application {

void PuzzleCursor::Initialize(int startRow, int startCol, int boardSize, float easingDuration,
	const std::string& commandPrefix) {
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
	isHoldingAction_ = false;

	cmdUp_ = commandPrefix + "Up";
	cmdDown_ = commandPrefix + "Down";
	cmdLeft_ = commandPrefix + "Left";
	cmdRight_ = commandPrefix + "Right";
	cmdActionHold_ = commandPrefix + "ActionHold";
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

	isHoldingAction_ = inputCommand->Evaluate(cmdActionHold_).Triggered();

	int dirUp = inputCommand->Evaluate(cmdUp_).Triggered() ? 1 : 0;
	int dirDown = inputCommand->Evaluate(cmdDown_).Triggered() ? 1 : 0;
	int dirLeft = inputCommand->Evaluate(cmdLeft_).Triggered() ? 1 : 0;
	int dirRight = inputCommand->Evaluate(cmdRight_).Triggered() ? 1 : 0;

	if (isHoldingAction_ && !panelsAnimating) {
		if (dirUp) {
			hasMoveAction_ = true;
			moveActionDir_ = 0;
		} else if (dirDown) {
			hasMoveAction_ = true;
			moveActionDir_ = 1;
		} else if (dirLeft) {
			hasMoveAction_ = true;
			moveActionDir_ = 2;
		} else if (dirRight) {
			hasMoveAction_ = true;
			moveActionDir_ = 3;
		}
	} else if (!isHoldingAction_) {
		int newRow = row_;
		int newCol = col_;

		if (boardSize_ > 0) {
			if (dirUp) newRow = (row_ - 1 + boardSize_) % boardSize_;
			if (dirDown) newRow = (row_ + 1) % boardSize_;
			if (dirLeft) newCol = (col_ - 1 + boardSize_) % boardSize_;
			if (dirRight) newCol = (col_ + 1) % boardSize_;
		}

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

bool PuzzleCursor::StepMove(int direction) {
	if (isMoving_) return false;
	if (boardSize_ <= 0) return false;

	int newRow = row_;
	int newCol = col_;
	switch (direction) {
	case 0: newRow = (row_ - 1 + boardSize_) % boardSize_; break;
	case 1: newRow = (row_ + 1) % boardSize_; break;
	case 2: newCol = (col_ - 1 + boardSize_) % boardSize_; break;
	case 3: newCol = (col_ + 1) % boardSize_; break;
	default: return false;
	}

	if (newRow == row_ && newCol == col_) return false;

	startRow_ = currentRow_;
	startCol_ = currentCol_;
	targetRow_ = static_cast<float>(newRow);
	targetCol_ = static_cast<float>(newCol);
	row_ = newRow;
	col_ = newCol;
	isMoving_ = true;
	moveTimer_ = 0.0f;
	return true;
}

bool PuzzleCursor::IsHoldingAction() const {
	return isHoldingAction_;
}

} // namespace Application
