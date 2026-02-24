#pragma once
namespace Application
{
	// ブロックスクロールを管理するクラス
	class BlockScroller
	{
	public:
		/// スクロールするスピード(毎秒)とスクロール幅のオフセットを指定して初期化
		void Initialize(float scrollSpeed,float scrollOffset); // 初期化
		/// スクロールを更新する。
		void Update(float deltaTime);

		/// スクロールが完了したフレームかどうかを返す。Update()を呼び出した後に呼び出すこと。
		bool IsScrollComplete() const;
		/// スクロール量を取得する。Update()を呼び出した後に呼び出すこと。
		float GetCurrentScroll() const { return currentScroll_; }

		void SetScrollSpeed(float scrollSpeed) { scrollSpeed_ = scrollSpeed; }

	private:
		float scrollSpeed_; // スクロール速度
		float scrollOffset_; // スクロールオフセット
		float currentScroll_; // 現在のスクロール量
		bool isScrollCompleteFrame_; // スクロールが完了したフレームかどうか
	};
}
