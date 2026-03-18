#pragma once
#include <KashipanEngine.h>
#include "BlockSprite.h"

namespace Application {
	class BoardSprite {
	public:
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,int w,int h);
		void Update(const std::vector<int>& board);

		// ある行のセルをずらす
		void ShiftRow(int rowIndex, float shiftAmount);
		// ある列のセルをずらす
		void ShiftColumn(int columnIndex, float shiftAmount);

		float GetCellSize() const { return cellSize_; }

		// セルの位置を取得する
		Vector2 GetCellPosition(int x, int y) const;

		// 盤面全体を移動させる
		KashipanEngine::Sprite* GetAnchorSprite() const { return boardAnchorSprite_; }

	private:
		std::vector<int> oldBoardState_;
		float cellSize_ = 0.0f;
		int maxWidth_ = 0;
		int maxHeight_ = 0;

		KashipanEngine::Sprite* boardAnchorSprite_ = nullptr;
		KashipanEngine::Sprite* backgroundSprite_ = nullptr;
		std::vector<BlockSprite> cellSprites_;
	};
}