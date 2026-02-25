#pragma once
#include <KashipanEngine.h>

namespace Application
{
	/// カーソルの位置を管理するクラス
	class Cursor final
	{
	public:
		void Initialize(int32_t rows, int32_t cols,int32_t maxRow,int32_t maxCol);
		
		/// カーソルの位置を更新します。行と列は0から始まります。
		void UpdatePosition(KashipanEngine::InputCommand* Inputcommand);
		
		/// カーソルの現在の行と列を取得します。
		std::pair<int32_t, int32_t> GetPosition() const { return {row_, col_}; }
		void SetPosition(int32_t row, int32_t col) {
			row_ = std::clamp(row, 0, maxRows_ - 1);
			col_ = std::clamp(col, 0, maxCols_ - 1);
		}
		
	private:
		int32_t row_; // カーソルの行位置
		int32_t col_; // カーソルの列位置

		int32_t maxRows_; // カーソルが移動できる最大行数
		int32_t maxCols_; // カーソルが移動できる最大列
	};
}