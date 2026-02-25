#include "HandBlockContainer.h"

void Application::HandBlockContainer::Initialize(int32_t maxBlocks) {
	maxBlocks_ = maxBlocks;
	handBlocks_.clear();
	handBlocks_.resize(maxBlocks_, 0);

	// 1~3をランダムに生成して手持ちのブロックを初期化
	for (int i = 0; i < maxBlocks_; i++) {
		handBlocks_[i] = rand() % 3 + 1;
	}
}

int32_t Application::HandBlockContainer::PopHandBlock() {
	// 手持ちのブロックの先頭を取り出す
	int32_t block = handBlocks_[0];
	for (size_t i = 1; i < handBlocks_.size(); i++) {
		handBlocks_[i - 1] = handBlocks_[i];
	}
	// 最後のブロックは新しいランダムなブロックで埋める
	handBlocks_[handBlocks_.size() - 1] = 0;
	return block;
}

void Application::HandBlockContainer::PopFront() {
	// 手持ちのブロックの先頭を消す（0にする）
	for (size_t i = 1; i < handBlocks_.size(); i++) {
		handBlocks_[i - 1] = handBlocks_[i];
	}
	// 最後のブロックは新しいランダムなブロックで埋める
	handBlocks_[handBlocks_.size() - 1] = 0;
}

void Application::HandBlockContainer::ReloadBlocks(float delta) {
	if (reloadTimer_ == 0.0f) {
		// 手持ちの0ブロックをすべて新しいランダムなブロックで埋める
		for (int i = 0; i < maxBlocks_; i++) {
			if (handBlocks_[i] == 0) {
				handBlocks_[i] = rand() % 3 + 1;
			}
		}

	} else {

		reloadTimer_ -= delta;
		if (reloadTimer_ <= 0.0f) {
			reloadTimer_ = 0.0f;
		}
	}
}
