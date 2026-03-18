#include "Cursor.h"
using namespace Application;
#include <algorithm>

void Cursor::Initialize(int maxX, int maxY) {
	x_ = 0;
	y_ = 0;
	maxX_ = maxX;
	maxY_ = maxY;
	isSelected_ = false;
}

void Cursor::Update() {
	// カーソルの選択処理
	if (selectFunction_ && selectFunction_()) {
		isSelected_ = true;
		return; // 選択された場合は移動処理をスキップ
	} else {
		isSelected_ = false;
	}

	// カーソルの移動処理
	if (moveUpFunction_ && moveUpFunction_()) {
		if (y_ < maxY_ - 1) {
			++y_;
		} else {
			y_ = 0;
		}
	}
	// カーソルの移動処理
	if (moveDownFunction_ && moveDownFunction_()) {
		if (y_ > 0) {
			--y_;
		} else {
			y_ = maxY_ - 1;
		}
	}
	// カーソルの移動処理
	if (moveLeftFunction_ && moveLeftFunction_()) {
		if (x_ > 0) {
			--x_;
		} else {
			x_ = maxX_ - 1;
		}
	}
	// カーソルの移動処理
	if (moveRightFunction_ && moveRightFunction_()) {
		if (x_ < maxX_ - 1) {
			++x_;
		} else {
			x_ = 0;
		}
	}
}

int Application::Cursor::GetMoveX() {
	int moveX = 0;
	if (moveLeftFunction_ && moveLeftFunction_()) {
		moveX -= 1;
	}
	if (moveRightFunction_ && moveRightFunction_()) {
		moveX += 1;
	}
	return moveX;
}

int Application::Cursor::GetMoveY() {
	int moveY = 0;
	if (moveUpFunction_ && moveUpFunction_()) {
		moveY -= 1;
	}
	if (moveDownFunction_ && moveDownFunction_()) {
		moveY += 1;
	}
	return moveY;
}
