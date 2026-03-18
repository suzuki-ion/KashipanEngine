#include "BlockSprite.h"
#include <MatsumotoUtility.h>

void Application::BlockSprite::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc) {
	blockSprite_ = createSpriteFunc("Block", "Cell_0.png", KashipanEngine::DefaultSampler::LinearClamp);
}

void Application::BlockSprite::Update() {
	// ブロックの状態に応じてスプライトの表示を切り替える処理をここに追加
}

void Application::BlockSprite::SetBlockState(int blockType) {
	// blockTypeに応じてスプライトのテクスチャを変更する処理
	if (!blockSprite_) return;
	std::string textureName = "Cell_" + std::to_string(blockType) + ".png";
	Application::MatsumotoUtility::SetTextureToSprite(blockSprite_, textureName);
}

void Application::BlockSprite::ParentTo(KashipanEngine::Sprite* parent) {
	if (blockSprite_ && parent) {
		Application::MatsumotoUtility::ParentSpriteToSprite(blockSprite_, parent);
	}
}

void Application::BlockSprite::SetPosition(const Vector3& position) {
	if (blockSprite_) {
		Application::MatsumotoUtility::SetTranslateToSprite(blockSprite_, position);
	}
}

float Application::BlockSprite::GetSize() const {
	return blockSprite_ ? Application::MatsumotoUtility::GetTextureSizeFromSprite(blockSprite_).x : 0.0f;
}

void Application::BlockSprite::MoveTo(const Vector3& targetPosition, float transitionSpeed) {
	if (blockSprite_) {
		Application::MatsumotoUtility::SimpleEaseSpriteMove(blockSprite_, targetPosition, transitionSpeed);
	}
}

void Application::BlockSprite::AddMove(const Vector3& deltaPosition) {
	if (blockSprite_) {
		Vector3 currentPosition = Application::MatsumotoUtility::GetTranslateFromSprite(blockSprite_);
		Vector3 targetPosition = currentPosition + deltaPosition;
		Application::MatsumotoUtility::SetTranslateToSprite(blockSprite_, targetPosition);
	}
}
