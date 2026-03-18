#include "MatchFinder.h"
#include <algorithm>

void Application::MatchFinder::Initialize(int width, int height) {
	width_ = width;
	height_ = height;
}

std::vector<std::vector<std::pair<int, int>>> Application::MatchFinder::FindThreeColorMatch(std::vector<int> board) {
	std::vector<std::vector<std::pair<int, int>>> matches;

	// 盤面を走査して、同じ色が3つ以上並んでいる場所を見つける
	// --- 水平方向のチェック ---
	for (int y = 0; y < 6; ++y) {
		for (int x = 0; x < 6; ) {
			int currentColor = board[y * 6 + x];
			if (currentColor == 0) {
				++x; // 空のセルの場合は次へ
				continue;
			}

			int matchLength = 1;
			// 右隣が同じ色である限り長さを伸ばす
			while (x + matchLength < 6 && board[y * 6 + (x + matchLength)] == currentColor) {
				matchLength++;
			}

			// 3つ以上繋がっていたらマッチとして登録
			if (matchLength >= 3) {
				std::vector<std::pair<int, int>> currentMatch;
				for (int i = 0; i < matchLength; ++i) {
					currentMatch.push_back({ x + i, y });
				}
				matches.push_back(currentMatch);
			}

			// 走査済みのブロックをスキップする
			x += matchLength;
		}
	}

	// --- 垂直方向のチェック ---
	for (int x = 0; x < 6; ++x) {
		for (int y = 0; y < 6; ) {
			int currentColor = board[y * 6 + x];
			if (currentColor == 0) {
				++y; // 空のセルの場合は次へ
				continue;
			}

			int matchLength = 1;
			// 下隣が同じ色である限り長さを伸ばす
			while (y + matchLength < 6 && board[(y + matchLength) * 6 + x] == currentColor) {
				matchLength++;
			}

			// 3つ以上繋がっていたらマッチとして登録
			if (matchLength >= 3) {
				std::vector<std::pair<int, int>> currentMatch;
				for (int i = 0; i < matchLength; ++i) {
					currentMatch.push_back({ x, y + i });
				}
				matches.push_back(currentMatch);
			}

			// 走査済みのブロックをスキップする
			y += matchLength;
		}
	}

	return matches;
}

std::vector<std::vector<std::pair<int, int>>> Application::MatchFinder::FindThreeColorMatchExceptRow(int rowIndex, std::vector<int> board)
{
	std::vector<std::vector<std::pair<int, int>>> matches;

	// 盤面を走査して、同じ色が3つ以上並んでいる場所を見つける
	// --- 水平方向のチェック ---
	for (int y = 0; y < 6; ++y) {
		for (int x = 0; x < 6; ) {
			int currentColor = board[y * 6 + x];
			if (currentColor == 0) {
				++x; // 空のセルの場合は次へ
				continue;
			}

			if(y == rowIndex) {
				break; // 指定された行はスキップ
			}

			int matchLength = 1;
			// 右隣が同じ色である限り長さを伸ばす
			while (x + matchLength < 6 && board[y * 6 + (x + matchLength)] == currentColor) {
				matchLength++;
			}

			// 3つ以上繋がっていたらマッチとして登録
			if (matchLength >= 3) {
				std::vector<std::pair<int, int>> currentMatch;
				for (int i = 0; i < matchLength; ++i) {
					currentMatch.push_back({ x + i, y });
				}
				matches.push_back(currentMatch);
			}

			// 走査済みのブロックをスキップする
			x += matchLength;
		}
	}

	// --- 垂直方向のチェック ---
	for (int x = 0; x < 6; ++x) {
		for (int y = 0; y < 6; ) {
			int currentColor = board[y * 6 + x];
			if (currentColor == 0) {
				++y; // 空のセルの場合は次へ
				continue;
			}

			int matchLength = 1;
			// 下隣が同じ色である限り長さを伸ばす
			while (y + matchLength < 6 && board[(y + matchLength) * 6 + x] == currentColor) {
				matchLength++;
			}

			// 3つ以上繋がっていたらマッチとして登録
			if (matchLength >= 3) {
				std::vector<std::pair<int, int>> currentMatch;
				for (int i = 0; i < matchLength; ++i) {
					currentMatch.push_back({ x, y + i });
				}
				matches.push_back(currentMatch);
			}

			// 走査済みのブロックをスキップする
			y += matchLength;
		}
	}

	return matches;
}

bool Application::MatchFinder::IsBlockInMatch(int x, int y, std::vector<int> board) {
	int index = y * width_ + x;

	// 水平方向のチェック
	int currentColor = board[index];
	if (currentColor != -1) {
		int matchLength = 1;
		// 右隣が同じ色である限り長さを伸ばす
		while (x + matchLength < width_ && board[y * width_ + (x + matchLength)] == currentColor) {
			matchLength++;
		}
		// 左隣が同じ色である限り長さを伸ばす
		while (x - matchLength >= 0 && board[y * width_ + (x - matchLength)] == currentColor) {
			matchLength++;
		}
		if (matchLength >= 3) {
			return true;
		}
	}

	// 垂直方向のチェック
	if (currentColor != -1) {
		int matchLength = 1;
		// 下隣が同じ色である限り長さを伸ばす
		while (y + matchLength < height_ && board[(y + matchLength) * width_ + x] == currentColor) {
			matchLength++;
		}
		// 上隣が同じ色である限り長さを伸ばす
		while (y - matchLength >= 0 && board[(y - matchLength) * width_ + x] == currentColor) {
			matchLength++;
		}
		if (matchLength >= 3) {
			return true;
		}
	}

	return false;
}

std::vector<std::pair<int, int>> Application::MatchFinder::FindMatchFromBlock(int x, int y, std::vector<int> board) {
	std::vector<std::pair<int, int>> matches;
	int index = y * width_ + x;
	int currentColor = board[index];

	// 水平方向のチェック
	if (currentColor != -1) {
		int matchLength = 1;
		// 右隣が同じ色である限り長さを伸ばす
		while (x + matchLength < width_ && board[y * width_ + (x + matchLength)] == currentColor) {
			matchLength++;
		}
		// 左隣が同じ色である限り長さを伸ばす
		while (x - matchLength >= 0 && board[y * width_ + (x - matchLength)] == currentColor) {
			matchLength++;
		}
		if (matchLength >= 3) {
			for (int i = -matchLength + 1; i < matchLength; ++i) {
				if (x + i >= 0 && x + i < width_) {
					matches.push_back({ x + i, y });
				}
			}
		}
	}

	// 垂直方向のチェック
	if (currentColor != -1) {
		int matchLength = 1;
		// 下隣が同じ色である限り長さを伸ばす
		while (y + matchLength < height_ && board[(y + matchLength) * width_ + x] == currentColor) {
			matchLength++;
		}
		// 上隣が同じ色である限り長さを伸ばす
		while (y - matchLength >= 0 && board[(y - matchLength) * width_ + x] == currentColor) {
			matchLength++;
		}
		if (matchLength >= 3) {
			for (int i = -matchLength + 1; i < matchLength; ++i) {
				if (y + i >= 0 && y + i < height_) {
					matches.push_back({ x, y + i });
				}
			}
		}
	}

	return matches;
}
