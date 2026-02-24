#include "BlockContainer.h"
#include "BlockSpriteContainer.h"

void Application::BlockContainer::Initialize(int32_t rows, int32_t cols)
{
	rows_ = rows;
	cols_ = cols;
	overflowCount_ = 0;

	blocks_.resize(rows);
	for (auto& row : blocks_) {
		row.resize(cols, 0); // 0で初期化
	}
}

void Application::BlockContainer::ResetOverflowCount()
{
	overflowCount_ = 0;
}

void Application::BlockContainer::ResetBlocks()
{
	for (auto& row : blocks_) {
		std::fill(row.begin(), row.end(), 0); // 全てのブロックを0にリセット
	}
}

void Application::BlockContainer::SetBlock(int32_t row, int32_t col, int32_t value)
{
	if (row >= 0 && row < static_cast<int32_t>(blocks_.size()) &&
		col >= 0 && col < static_cast<int32_t>(blocks_[row].size())) {
		blocks_[row][col] = value;
	}
}

void Application::BlockContainer::PushBlock(int32_t row, int32_t col, int32_t value)
{
	if (row >= 0 && row < static_cast<int32_t>(blocks_.size()-1) &&
		col >= 0 && col < static_cast<int32_t>(blocks_[row].size())) {
		// 指定された位置から上に向かってブロックを押し上げる
		for (int32_t r = row; r < static_cast<int32_t>(blocks_.size()); ++r) {
			// 0以外のブロックが最上段に達した場合はオーバーフローとみなす
			if (r == static_cast<int32_t>(blocks_.size()) - 1) {
				if (blocks_[r][col] != 0) {
					overflowCount_++;
				}
				break;
			}
			// 上の行のブロックを現在の行に移動
			blocks_[r][col] = blocks_[r + 1][col];
		}

		// 最下段に新しいブロックを差し込む
		blocks_[row][col] = value;
	}
}

void Application::BlockContainer::PushRow(const std::vector<int32_t>& newRow)
{
	if (newRow.size() != static_cast<size_t>(cols_)) {
		return; // 列数が一致しない場合は処理しない
	}
	// 全ての行を上に押し上げる
	for (int32_t r = static_cast<int32_t>(blocks_.size()) - 1; r > 0; --r) {
		for (int32_t c = 0; c < static_cast<int32_t>(blocks_[r].size()); ++c) {
			// 最上段の0以外のブロックはオーバーフローとみなす
			if (r == static_cast<int32_t>(blocks_.size()) - 1) {
				if (blocks_[r][c] != 0) {
					overflowCount_++;
				}
			}
			// 下の行のブロックを現在の行に移動
			blocks_[r][c] = blocks_[r - 1][c];
		}
	}
	// 最下段に新しい行を差し込む
	for (int32_t c = 0; c < static_cast<int32_t>(blocks_[0].size()); ++c) {
		blocks_[0][c] = newRow[c];
	}
}

int32_t Application::BlockContainer::GetBlock(int32_t row, int32_t col) const
{
	if (row >= 0 && row < static_cast<int32_t>(blocks_.size()) &&
		col >= 0 && col < static_cast<int32_t>(blocks_[row].size())) {
		return blocks_[row][col];
	}
	return -1; // 範囲外の場合は-1を返す
}
