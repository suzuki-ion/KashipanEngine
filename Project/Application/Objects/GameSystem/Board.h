#pragma once
#include <vector>

namespace Application {
	// 盤面のデータを管理するクラス
	class Board {
	public:
		/// @brief 盤面を初期化する
		void Initialize(int width, int height);
		/// @brief 幅を取得する
		int GetWidth() const { return width_; }
		/// @brief 高さを取得する
		int GetHeight() const { return height_; }
		/// @brief 指定した位置のセルの値を取得する
		int GetCell(int x, int y) const { return data_[y * width_ + x]; }
		/// @brief 指定した位置のセルの値を設定する
		void SetCell(int x, int y, int value) { data_[y * width_ + x] = value; }
		/// @brief 盤面のデータを取得する
		const std::vector<int>& GetBoardData() const { return data_; }

		/// @brief ある行をシフトする
		void ShiftRow(int rowIndex, int shiftAmount);
		/// @brief ある列をシフトする
		void ShiftColumn(int columnIndex, int shiftAmount);

		/// @brief 盤面をランダムな数値でリセットする
		void Reset();

	private:
		int width_;
		int height_;
		std::vector<int> data_;
	};
}