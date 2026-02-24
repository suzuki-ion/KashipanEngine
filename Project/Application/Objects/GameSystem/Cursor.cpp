#include "Cursor.h"

void Application::Cursor::Initialize(int32_t rows, int32_t cols, int32_t maxRow, int32_t maxCol)
{
	row_ = rows;
	col_ = cols;
	maxRows_ = maxRow;
	maxCols_ = maxCol;
}

void Application::Cursor::UpdatePosition(KashipanEngine::InputCommand* Inputcommand)
{
	if (Inputcommand->Evaluate("MoveUp").Triggered()) {
		row_++;
	}
	if (Inputcommand->Evaluate("MoveDown").Triggered()) {
		row_--;
	}
	if (Inputcommand->Evaluate("MoveLeft").Triggered()) {
		col_--;
	}
	if (Inputcommand->Evaluate("MoveRight").Triggered()) {
		col_++;
	}

	// カーソルの位置を最大値と最小値の範囲内に制限
	row_ = row_ % maxRows_;
	if (row_ < 0) {
		row_ += maxRows_;
	}
	col_ = col_ % maxCols_;
	if (col_ < 0) {
		col_ += maxCols_;
	}
}
