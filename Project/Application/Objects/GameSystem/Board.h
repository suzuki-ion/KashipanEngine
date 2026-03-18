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

		/// @brief 削除するセルを登録する
		void RegisterEreaseCells(const std::vector<std::vector<std::pair<int, int>>>& cells) { ereaseCells_ = cells; }
		/// @brief 削除するセル配列通りに盤面を更新する
		void EreaseCells();
		/// @brief ある行を除いてセルを削除する
		void EreaseCellsExceptRow(int rowIndex);
		/// @brief 削除するセルが登録されているか
		bool IsEreaseCellsEmpty() const { return ereaseCells_.empty(); }

		/// @brief ある行を基準に重力を適用してセルを落とす
		void ApplyGravity(int baseRowIndex);

		/// @brief 盤面の0を埋める
		void FillEmptyCells();

		float NoiseRatio() const;

		/// @brief 盤面内の空のセルの位置を取得する
		std::vector<std::pair<int, int>> FindEmptyCells() const;

		int GetEraseCount() const { return eraseCount_; }
		int GetNoiseEraseCount() const { return noiseEreaseCount_; }
		void ResetEraseCounts() { eraseCount_ = 0; noiseEreaseCount_ = 0; }

	private:
		int width_;
		int height_;
		std::vector<int> data_;

		int eraseCount_ = 0;
		int noiseEreaseCount_ = 0;

		std::vector<std::vector<std::pair<int, int>>> ereaseCells_;
	};
}