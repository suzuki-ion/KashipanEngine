#include "BlockScroller.h"

void Application::BlockScroller::Initialize(float scrollSpeed, float scrollOffset)
{
	scrollSpeed_ = scrollSpeed;
	scrollOffset_ = scrollOffset;
	isScrollCompleteFrame_ = false;
	currentScroll_ = 0.0f;
}

void Application::BlockScroller::Update(float deltaTime)
{
	isScrollCompleteFrame_ = false; // スクロール完了フラグをリセット

	currentScroll_ += scrollSpeed_ * deltaTime; // スクロールオフセットを更新
	if (currentScroll_ >= scrollOffset_) { // スクロールが完了したかどうかをチェック
		currentScroll_ = 0.0f;// スクロール量をリセット
		isScrollCompleteFrame_ = true; // スクロール完了フラグを立てる
	}
}

bool Application::BlockScroller::IsScrollComplete() const
{
	return isScrollCompleteFrame_; // スクロールが完了したフレームかどうかを返す
}