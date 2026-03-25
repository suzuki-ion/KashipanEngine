#pragma once
#include <KashipanEngine.h>
#include "BlockSprite.h"

namespace Application {
	class BoardSprite {
	public:
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,int w,int h);
		void Update(const std::vector<int>& board, float delta);

		// ある行のセルをずらす
		void ShiftRow(int rowIndex, float shiftAmount);
		// ある列のセルをずらす
		void ShiftColumn(int columnIndex, float shiftAmount);

		float GetCellSize() const { return cellSize_; }

		// セルの位置を取得する
		Vector2 GetCellPosition(int x, int y) const;

		// 指定のセルを小さくする
		void ShrinkCell(int x, int y, float scaleMultiplier);
		// 指定のセルを小さくする
		void ShrinkCells(const std::vector<std::pair<int, int>>& cells, float scaleMultiplier) {
			for (const auto& cell : cells) {
				ShrinkCell(cell.first, cell.second, scaleMultiplier);
			}
		}

		// マッチしているセルを登録する
		void RegisterMatchCells(const std::vector<std::vector<std::pair<int, int>>>& matchCells);

		// 盤面全体を移動させる
		KashipanEngine::Sprite* GetAnchorSprite() const { return boardAnchorSprite_; }

		// マッチアニメーションが進行中か
		bool IsAnimatingMatch() const { return isAnimatingMatch_; }

		void ShakeBoard() {
			shakeTimer_ = shakeDuration_; // 揺らすアニメーションを開始
		}

	private:
		std::vector<int> oldBoardState_;
		float cellSize_ = 0.0f;
		int maxWidth_ = 0;
		int maxHeight_ = 0;
		bool isAnimatingMatch_ = false;

		KashipanEngine::Sprite* boardAnchorSprite_ = nullptr;
		KashipanEngine::Sprite* backgroundSprite_ = nullptr;
		std::vector<BlockSprite> cellSprites_;

		float matchAnimationDuration_ = 0.5f;
		float matchAnimationTimer_ = 0.0f;
		std::vector<std::vector<std::pair<int, int>>> ongoingMatchAnimations_;

		float shakeDuration_ = 0.5f;
		float shakeTimer_ = 0.0f;
	};
}