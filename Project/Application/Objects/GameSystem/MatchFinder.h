#pragma once
#include <vector>
namespace Application {
	// ある形のパズルが盤面内に揃っているかを管理するクラス
	class MatchFinder {
	public:
		/// @brief 盤面の幅と高さを設定する関数
		void Initialize(int width,int height);

		/// @brief 盤面内に同じ色が3つ以並んでいる場所を見つける関数
		std::vector<std::vector<std::pair<int, int>>> FindThreeColorMatch(std::vector<int> board);

		/// @brief ある行の横一列を除いて同じ色が3つ以上並んでいる場所を見つける関数
		std::vector<std::vector<std::pair<int, int>>> FindThreeColorMatchExceptRow(int rowIndex, std::vector<int> board);
		
		/// @brief ある地点のブロックが3つ以上繋がっているかをチェックする関数
		bool IsBlockInMatch(int x, int y, std::vector<int> board);

		/// @brief ある地点から同じ色が3つ以上並んでいる場所を見つける関数
		std::vector<std::pair<int, int>> FindMatchFromBlock(int x, int y, std::vector<int> board);

	private:
		int width_ = 0;
		int height_ = 0;

	};
}