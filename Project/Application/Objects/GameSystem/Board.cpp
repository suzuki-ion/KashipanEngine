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
