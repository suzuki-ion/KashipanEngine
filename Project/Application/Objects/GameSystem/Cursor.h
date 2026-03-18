#pragma once
#include <functional>

namespace Application {
	// カーソルの操作、状態を管理するクラス
	class Cursor {
	public:
		/// @brief カーソルを初期化する
		void Initialize(int maxX, int maxY);
		/// @brief カーソルの状態を更新する
		void Update();

		/// @brief カーソルの選択状態を設定する
		void SetMoveUpFunction(std::function<bool()> func) { moveUpFunction_ = std::move(func); }
		void SetMoveDownFunction(std::function<bool()> func) { moveDownFunction_ = std::move(func); }
		void SetMoveLeftFunction(std::function<bool()> func) { moveLeftFunction_ = std::move(func); }
		void SetMoveRightFunction(std::function<bool()> func) { moveRightFunction_ = std::move(func); }

		/// @brief カーソルの選択関数を設定する
		void SetSelectFunction(std::function<bool()> func) { selectFunction_ = std::move(func); }

		/// @brief カーソルの位置を取得する
		int GetX() const { return x_; }
		int GetY() const { return y_; }
		/// @brief カーソルの選択状態を取得する
		bool IsSelected() const { return isSelected_; }
		/// @brief カーソルの移動量を取得する
		int GetMoveX();
		int GetMoveY();

	private:
		// カーソルの位置
		int x_ = 0;
		int y_ = 0;

		// カーソルの移動限界
		int maxX_ = 0;
		int maxY_ = 0;

		// カーソルが選択状態かどうか
		bool isSelected_ = false;

		// カーソルの移動関数
		std::function<bool()> moveUpFunction_;
		std::function<bool()> moveDownFunction_;
		std::function<bool()> moveLeftFunction_;
		std::function<bool()> moveRightFunction_;

		// カーソルの選択関数
		std::function<bool()> selectFunction_;
	};
}