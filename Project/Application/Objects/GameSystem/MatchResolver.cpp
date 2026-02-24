#include "MatchResolver.h"

void Application::MatchResolver::Initialize()
{
	haveMatch_ = false;
	matchStopDuration_ = 1.5f; 
}

void Application::MatchResolver::Update(float delta)
{
	isCompleteMatchStopFrame_ = false; // マッチ停止完了フレームのフラグをリセット

	if(haveMatch_) {
		matchStopTimer_ += delta; // マッチがあるときはタイマーを進める
		if(matchStopTimer_ >= matchStopDuration_) {
			isCompleteMatchStopFrame_ = true; // マッチ停止が完了したフレームとする
			matchStopTimer_ = 0.0f; // タイマーをリセット
			haveMatch_ = false; // マッチがない状態に戻す
		}
	}
}

std::vector<std::pair<int32_t, int32_t>> Application::MatchResolver::ResolveMatches(const std::vector<std::vector<int32_t>>& blockContainer)
{
	// 0以外で縦または横に3つ以上連続しているブロックの位置を検出する
	std::vector<std::pair<int32_t, int32_t>> matchedPositions;
	int32_t rows = static_cast<int32_t>(blockContainer.size());
	int32_t cols = rows > 0 ? static_cast<int32_t>(blockContainer[0].size()) : 0;

	// 横方向のマッチを検出
	for (int32_t row = 0; row < rows; ++row) {
		int32_t count = 1;
		for (int32_t col = 1; col < cols; ++col) {
			if (blockContainer[row][col] != 0 && blockContainer[row][col] != 4 && blockContainer[row][col] == blockContainer[row][col - 1]) {
				count++;
			}
			else {
				if (count >= 3) {
					for (int32_t c = col - count; c < col; ++c) {
						matchedPositions.emplace_back(row, c);
					}
				}
				count = 1;
			}
		}
		if (count >= 3) {
			for (int32_t c = cols - count; c < cols; ++c) {
				matchedPositions.emplace_back(row, c);
			}
		}
	}

	// 縦方向のマッチを検出
	for (int32_t col = 0; col < cols; ++col) {
		int32_t count = 1;
		for (int32_t row = 1; row < rows; ++row) {
			if (blockContainer[row][col] != 0 && blockContainer[row][col] != 4 && blockContainer[row][col] == blockContainer[row - 1][col]) {
				count++;
			}
			else {
				if (count >= 3) {
					for (int32_t r = row - count; r < row; ++r) {
						matchedPositions.emplace_back(r, col);
					}
				}
				count = 1;
			}
		}
		if (count >= 3) {
			for (int32_t r = rows - count; r < rows; ++r) {
				matchedPositions.emplace_back(r, col);
			}
		}
	}

	if(haveMatch_ != !matchedPositions.empty()) {
		matchStopTimer_ = 0.0f; // マッチが新たに発生した場合はタイマーをリセット
	}
	haveMatch_ = !matchedPositions.empty(); // マッチがあるかどうかのフラグを更新

	return matchedPositions;
}
