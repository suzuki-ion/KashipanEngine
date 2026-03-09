#pragma once
#include <functional>

namespace Application
{
	/// 選ばれている数字を入力より管理するクラス
	class SelectNumderManager {
	public:
		void Initialize(
			std::function<bool()> upNumberFunc,
			std::function<bool()> downNumberFunc,
			std::function<bool()> submitNumberFunc,
			std::function<bool()> cancelNumberFunc);

		// 入力に応じて選択数字を更新する
		void Update();
		
		// maxNumberを設定して選択数字を初期化
		void Setup(int maxNumber);

		// 選んでいる数字を取得する
		int GetSelectNumber() const { return selectNumber_; }
		// 選択可能な最大数字を取得する
		int GetMaxNumber() const { return maxNumber_; }

	private:
		std::function<bool()> upNumberFunc_;
		std::function<bool()> downNumberFunc_;
		std::function<bool()> submitNumberFunc_;
		std::function<bool()> cancelNumberFunc_;

		int selectNumber_;
		int maxNumber_;
	};
}