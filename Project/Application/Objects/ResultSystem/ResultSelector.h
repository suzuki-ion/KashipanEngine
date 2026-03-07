#pragma once
#include <Objects/TitleSystem/SelectNumderManager.h>

namespace Application
{
	/// @brief リザルト画面で、選択している項目を管理するクラス
	class ResultSelector {
	public:
		void Initialize(
			std::function<bool()> upFunc,
			std::function<bool()> downFunc,
			std::function<bool()> submitFunc,
			std::function<bool()> cancelFunc);
		// 入力に応じて選択項目を更新する
		void Update();

		bool IsSelecting() const { return selecting_; }
		int GetSelectNumber() const { return selectNumderManager_.GetSelectNumber(); }

	private:
		std::function<bool()> upFunc_;
		std::function<bool()> downFunc_;
		std::function<bool()> submitFunc_;
		std::function<bool()> cancelFunc_;
		
		bool selecting_ = false;
		SelectNumderManager selectNumderManager_;
	};
}
