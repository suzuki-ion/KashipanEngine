#pragma once
#include <stdint.h>
#include <vector>

namespace Application {
	/// パズルのブロックを管理するクラス(0,0が左下、row,colが右上)
	class BlockContainer final {
		public:
			/// 指定された行数と列数で初期化します。
			void Initialize(int32_t rows, int32_t cols);
			/// 押し上げで範囲外に出たブロックの数をリセットします。
			void ResetOverflowCount();
			/// ブロックを全て0にリセットします。
			void ResetBlocks();

			/// 特定の位置にブロックを設定します。
			void SetBlock(int32_t row, int32_t col, int32_t value);
			/// 特定の位置にブロックを押し上げ、差し込みます。
			void PushBlock(int32_t row, int32_t col, int32_t value);
			/// 一番下の行に行を追加し、全ての行を押し上げます。
			void PushRow(const std::vector<int32_t>& newRow);

			/// ブロックコンテナの行数と列数を取得します。
			int GetRows() const { return rows_; }
			/// ブロックコンテナの行数と列数を取得します。
			int GetCols() const { return cols_; }
			/// 特定の位置のブロックの値を取得します。範囲外の場合は-1を返します。
			int32_t GetBlock(int32_t row, int32_t col) const;

			int32_t GetOverflowCount() const { return overflowCount_; }

			std::vector<std::vector<int32_t>> GetBlocks() { return blocks_; }

		private:
			// ブロックのコンテナ
			std::vector<std::vector<int32_t>> blocks_;
			int32_t rows_;// 行数
			int32_t cols_;// 列数
			
			int32_t overflowCount_;// 押し上げで範囲外に出たブロックの数
	};
}