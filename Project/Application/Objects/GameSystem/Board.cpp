#include "Board.h"
using namespace Application;

void Board::Initialize(int width, int height) {
	width_ = width;
	height_ = height;
	data_.resize(width * height, 0); // 初期値は0
}

void Application::Board::ShiftRow(int rowIndex, int shiftAmount) {
	if (rowIndex < 0 || rowIndex >= height_) {
		return; // 無効な行インデックス
	}
	std::vector<int> newRow(width_);
	for (int x = 0; x < width_; ++x) {
		int newX = (x + shiftAmount) % width_;
		if (newX < 0) {
			newX += width_; // 負のシフトに対応
		}
		newRow[newX] = GetCell(x, rowIndex);
	}
	for (int x = 0; x < width_; ++x) {
		SetCell(x, rowIndex, newRow[x]);
	}
}

void Application::Board::ShiftColumn(int columnIndex, int shiftAmount) {
	if (columnIndex < 0 || columnIndex >= width_) {
		return; // 無効な列インデックス
	}
	std::vector<int> newColumn(height_);
	for (int y = 0; y < height_; ++y) {
		int newY = (y + shiftAmount) % height_;
		if (newY < 0) {
			newY += height_; // 負のシフトに対応
		}
		newColumn[newY] = GetCell(columnIndex, y);
	}
	for (int y = 0; y < height_; ++y) {
		SetCell(columnIndex, y, newColumn[y]);
	}
}

void Application::Board::Reset() {
	for (int y = 0; y < height_; ++y) {
		for (int x = 0; x < width_; ++x) {
			SetCell(x, y, (rand() % 3) + 1);
		}
	}
}

void Application::Board::EreaseCells()
{
	if (ereaseCells_.empty()) return;

	for (std::vector<std::pair<int, int>>& cells : ereaseCells_) {
		// 4つ以上消した場合、ペナルティ
		if (cells.size() <= 3) {
			for (std::pair<int, int>& cell : cells) {
				int x = cell.first;
				int y = cell.second;
				SetCell(x, y, 0); // セルの値を0にして消す
			}
			eraseCount_ += static_cast<int>(cells.size());
		}
		else {
			for (std::pair<int, int>& cell : cells) {
				int x = cell.first;
				int y = cell.second;
				if (GetCell(x, y) == 4) {
					SetCell(x, y, 0); // ノイズを削除する
					noiseEreaseCount_++;
				}
				else {
					SetCell(x, y, 4); // ノイズを入れる
				}
			}
			eraseCount_ += static_cast<int>(cells.size());
		}
	}
	ereaseCells_.clear();
}

void Application::Board::EreaseCellsExceptRow(int rowIndex)
{
	if (ereaseCells_.empty()) return;
	for (std::vector<std::pair<int, int>>& cells : ereaseCells_) {
		for (std::pair<int, int>& cell : cells) {
			int x = cell.first;
			int y = cell.second;
			if (y != rowIndex) {
				SetCell(x, y, 0); // セルの値を0にして消す
			}
		}
	}
	ereaseCells_.clear();
}

void Application::Board::ApplyGravity(int baseRowIndex)
{
	for (int x = 0; x < width_; ++x) {
		// 1. baseRowIndex より上のブロックを baseRowIndex に向かって落とす
		int emptyCountUp = 0;
		for (int y = baseRowIndex; y >= 0; --y) {
			if (GetCell(x, y) == 0) {
				emptyCountUp++;
			}
			else if (emptyCountUp > 0) {
				SetCell(x, y + emptyCountUp, GetCell(x, y));
				SetCell(x, y, 0);
			}
		}

		// 2. baseRowIndex より下のブロックを baseRowIndex に向かって落とす
		int emptyCountDown = 0;
		for (int y = baseRowIndex + 1; y < height_; ++y) {
			if (GetCell(x, y) == 0) {
				emptyCountDown++;
			}
			else if (emptyCountDown > 0) {
				SetCell(x, y - emptyCountDown, GetCell(x, y));
				SetCell(x, y, 0);
			}
		}
	}
}

void Application::Board::FillEmptyCells()
{
	for (int y = 0; y < height_; ++y) {
		for (int x = 0; x < width_; ++x) {
			if (GetCell(x, y) == 0) {
				SetCell(x, y, (rand() % 3) + 1); // ランダムな数値で埋める
			}
		}
	}
}

float Application::Board::NoiseRatio() const
{
	float totalCells = static_cast<float>(width_ * height_);
	float noiseCells = 0.0f;
	for (int y = 0; y < height_; ++y) {
		for (int x = 0; x < width_; ++x) {
			if (GetCell(x, y) == 4) { // ノイズのセルは4と仮定
				noiseCells += 1.0f;
			}
		}
	}
	return noiseCells / totalCells;
}

std::vector<std::pair<int, int>> Application::Board::FindEmptyCells() const
{
	std::vector<std::pair<int, int>> emptyCells;
	for (int y = 0; y < height_; ++y) {
		for (int x = 0; x < width_; ++x) {
			if (GetCell(x, y) == 0) {
				emptyCells.emplace_back(x, y); // 空のセルの位置を追加
			}
		}
	}
	return emptyCells;
}
