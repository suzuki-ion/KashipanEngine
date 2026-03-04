#include "Objects/Puzzle/PuzzleGoal.h"
#include "Objects/Puzzle/PuzzleBoard.h"

namespace Application {

void PuzzleGoal::Initialize() {
	currentCombo_ = 0;
	totalCombo_ = 0;
}

void PuzzleGoal::AddCombo(int count) {
	currentCombo_ += count;
	totalCombo_ += count;
}

} // namespace Application
