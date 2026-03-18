#include "BoardSprite.h"
#include <MatsumotoUtility.h>

void Application::BoardSprite::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc, int w, int h) {
	boardAnchorSprite_ = createSpriteFunc("BoardAnchor", "white1x1.png",KashipanEngine::DefaultSampler::LinearClamp);
	maxHeight_ = h;
	maxWidth_ = w;

	matchAnimationDuration_ = 0.2f;
	matchAnimationTimer_ = 0.0f;

	backgroundSprite_ = createSpriteFunc("BoardBackground", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);
	// セルのスプライトを生成
	for (int i = 0; i < w * h; ++i) {
		BlockSprite cellSprite;
		cellSprite.Initialize(createSpriteFunc);
		cellSprites_.push_back(cellSprite);
	}

	// セルをboardAnchorSprite_の位置を中心として配置
	if (cellSprites_.empty()) return;
	cellSize_ = cellSprites_[0].GetSize();
	int index = 0;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			float posX = (x - w / 2.0f + 0.5f) * cellSize_;
			float posY = (y - h / 2.0f + 0.5f) * cellSize_;
			cellSprites_[index].SetPosition(Vector3(posX, posY, 0.0f));
			++index;
		}
	}

	// 背景をペアレント
	Application::MatsumotoUtility::ParentSpriteToSprite(backgroundSprite_, boardAnchorSprite_);
	// セルをペアレント
	for (BlockSprite& cellSprite : cellSprites_) {
		cellSprite.ParentTo(boardAnchorSprite_);
	}
}

void Application::BoardSprite::Update(const std::vector<int>& board, float delta) {
	// セルのスプライトを更新
	for (BlockSprite& cellSprite : cellSprites_) {
		cellSprite.Update();
	}

	// セルの状態に応じてスプライトの表示を切り替え
	{
		int index = 0;
		for (int y = 0; y < maxHeight_; ++y) {
			for (int x = 0; x < maxWidth_; ++x) {
				float posX = (x - maxWidth_ / 2.0f + 0.5f) * cellSize_;
				float posY = (y - maxHeight_ / 2.0f + 0.5f) * cellSize_;
				cellSprites_[index].MoveTo(Vector3(posX, posY, 0.0f), 0.3f);
				if (index < board.size()) {
					cellSprites_[index].SetBlockState(board[index]);
				}
				++index;
			}
		}
	}

	// セルの状態を保存
	oldBoardState_ = board;

	// マッチしているセルのアニメーションを更新
	isAnimatingMatch_ = !ongoingMatchAnimations_.empty();
	if (!isAnimatingMatch_) return;
	if (matchAnimationTimer_ <= 0.0f) {
		matchAnimationTimer_ = matchAnimationDuration_;

		// マッチしている配列最後から順番にアニメーションを開始
		std::vector <std::pair<int, int>> cellsToAnimate = ongoingMatchAnimations_.back();
		ongoingMatchAnimations_.pop_back();
		for(std::pair<int, int> cell : cellsToAnimate) {
			int x = cell.first;
			int y = cell.second;
			int index = y * maxWidth_ + x;
			if (index < cellSprites_.size()) {
				cellSprites_[index].AddScale(2.0f); // 拡大アニメーションを追加
				cellSprites_[index].AddRotation(3.14f * 2.0f); // 回転アニメーションを追加
			}
		}
	} else {
		matchAnimationTimer_ -= delta;
	}
}

void Application::BoardSprite::ShiftRow(int rowIndex, float shiftAmount) {
	if (rowIndex < 0 || rowIndex >= maxHeight_) return;
	// 指定された行のセルをずらす
	for (int x = 0; x < maxWidth_; ++x) {
		int index = rowIndex * maxWidth_ + x;
		if (index < cellSprites_.size()) {
			cellSprites_[index].AddMove(Vector3(shiftAmount, 0.0f, 0.0f));
		}
	}
}

void Application::BoardSprite::ShiftColumn(int columnIndex, float shiftAmount) {
	if (columnIndex < 0 || columnIndex >= maxWidth_) return;
	// 指定された列のセルをずらす
	for (int y = 0; y < maxHeight_; ++y) {
		int index = y * maxWidth_ + columnIndex;
		if (index < cellSprites_.size()) {
			cellSprites_[index].AddMove(Vector3(0.0f, shiftAmount, 0.0f));
		}
	}
}

Vector2 Application::BoardSprite::GetCellPosition(int x, int y) const {
	float posX = (x - maxWidth_ / 2.0f + 0.5f) * cellSize_;
	float posY = (y - maxHeight_ / 2.0f + 0.5f) * cellSize_;
	return Vector2(posX, posY);
}

void Application::BoardSprite::RegisterMatchCells(const std::vector<std::vector<std::pair<int, int>>>& matchCells) {
	ongoingMatchAnimations_ = matchCells; // マッチしているセルの位置を保存
}