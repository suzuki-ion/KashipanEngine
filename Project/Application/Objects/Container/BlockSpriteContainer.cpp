#include "BlockSpriteContainer.h"

using namespace KashipanEngine;
using namespace Application;

void BlockSpriteContainer::Initialize(int32_t rows, int32_t cols)
{
	blockSprites_.resize(rows);
	for (auto& row : blockSprites_) {
		row.resize(cols, nullptr); // nullptrで初期化
	}
}

void BlockSpriteContainer::SetBlockSprite(int32_t row, int32_t col, KashipanEngine::Sprite* sprite)
{
	if (row >= 0 && row < static_cast<int32_t>(blockSprites_.size()) &&
		col >= 0 && col < static_cast<int32_t>(blockSprites_[row].size())) {
		blockSprites_[row][col] = sprite;
	}
}

KashipanEngine::Sprite* Application::BlockSpriteContainer::GetBlockSprite(int32_t row, int32_t col) const
{
	if (row >= 0 && row < static_cast<int32_t>(blockSprites_.size()) &&
		col >= 0 && col < static_cast<int32_t>(blockSprites_[row].size())) {
		return blockSprites_[row][col];
	}
	return nullptr; // 範囲外の場合はnullptrを返す
}

void Application::BlockSpriteContainer::RsetBlockPosition(const Vector2& position, const Vector2& size)
{
	int32_t rows = static_cast<int32_t>(blockSprites_.size());
	int32_t cols = rows > 0 ? static_cast<int32_t>(blockSprites_[0].size()) : 0;
	Vector2 centerOffset = Vector2(
		(cols) * size.x * 0.5f,
		(rows) * size.y * 0.5f);
	for (int32_t row = 0; row < rows; ++row) {
		for (int32_t col = 0; col < cols; ++col) {
			if (auto* sprite = GetBlockSprite(row, col)) {
				if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
					Vector2 blockPos = Vector2(
						col * size.x,
						row * size.y);
					tr->SetTranslate(Vector2(
						position.x - centerOffset.x + blockPos.x,
						position.y - centerOffset.y + blockPos.y));
					tr->SetScale(Vector2(size.x*0.9f, size.y*0.9f));
				}
			}
		}
	}
}

void Application::BlockSpriteContainer::SetBlockSpriteColor(int32_t row, int32_t col, const Vector4& color)
{
	if (auto* sprite = GetBlockSprite(row, col)) {
		if (auto* mat = sprite->GetComponent2D<Material2D>()) {
			mat->SetColor(color);
		}
	}
}

void Application::BlockSpriteContainer::SetBlockSpriteScale(int32_t row, int32_t col, const Vector2& scale)
{
	if (auto* sprite = GetBlockSprite(row, col)) {
		if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
			tr->SetScale(scale);
		}
	}
}

Vector2 Application::BlockSpriteContainer::GetBlockPosition(int32_t row, int32_t col) const
{
	Vector2 position;
	if (auto* sprite = GetBlockSprite(row, col)) {
		if (auto* tr = sprite->GetComponent2D<Transform2D>())
		{
			position = tr->GetTranslate();
		}
	}
	return position;
}
