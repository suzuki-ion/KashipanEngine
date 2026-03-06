#include "HandBlockSpriteContainer.h"
#include <Config/GameSceneConfig.h>

using namespace KashipanEngine;

void Application::HandBlockSpriteContainer::Initialize(int32_t maxBlocks) {
	maxBlocks_ = maxBlocks;
	handBlockSprites_.clear();
	handBlockSprites_.resize(maxBlocks_, nullptr);
}

void Application::HandBlockSpriteContainer::SetPosition(const Vector2& position) {
	Vector2 centerOffset = Vector2(
		(maxBlocks_)*Application::kBlockSize * 0.5f,
		Application::kBlockSize * 0.5f);
	for (int32_t i = 0; i < maxBlocks_; ++i) {
		if (auto* sprite = GetHandBlockSprite(i)) {
			if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
				Vector2 blockPos = Vector2(
					i * Application::kBlockSize,
					0.0f);
				tr->SetTranslate(Vector2(
					position.x - centerOffset.x + blockPos.x,
					position.y - centerOffset.y + blockPos.y));
				if (i == 0) {
					tr->SetScale(Vector2(Application::kBlockSize * 1.2f, Application::kBlockSize * 1.2f));
				} else {
					tr->SetScale(Vector2(Application::kBlockSize * 0.8f, Application::kBlockSize * 0.8f));
				}
			}
		}
	}
}

void Application::HandBlockSpriteContainer::SetHandBlockSprite(int32_t index, KashipanEngine::Sprite* sprite) {
	if (index >= 0 && index < maxBlocks_) {
		handBlockSprites_[index] = sprite;
	}
}

KashipanEngine::Sprite* Application::HandBlockSpriteContainer::GetHandBlockSprite(int32_t index) const {
	if (index >= 0 && index < maxBlocks_) {
		return handBlockSprites_[index];
	}
	return nullptr;
}

void Application::HandBlockSpriteContainer::SetHandBlockSpriteColor(int32_t index, const Vector4& color) {
	if (auto* sprite = GetHandBlockSprite(index)) {
		if (auto* mat = sprite->GetComponent2D<Material2D>()) {
			mat->SetColor(color);
		}
	}
}
