#include "BlockSprite.h"
#include <MatsumotoUtility.h>

void Application::BlockSprite::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc) {
	blockSprite_ = createSpriteFunc("Block", "Cell_0.png", KashipanEngine::DefaultSampler::LinearClamp);
}

void Application::BlockSprite::Update() {
	// スプライトの状態を戻す
	if (blockSprite_) {
		MatsumotoUtility::SimpleEaseSpriteFitToTexture(blockSprite_, 0.3f);
		MatsumotoUtility::SimpleEaseSpriteRotate(blockSprite_, Vector3(0.0f, 0.0f, 0.0f), 0.2f);
	}
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

void Application::BlockSprite::AddRotation(float deltaRotation) {
	if (blockSprite_) {
		Vector3 currentRotation = Application::MatsumotoUtility::GetRotationFromSprite(blockSprite_);
		Vector3 targetRotation = currentRotation + Vector3(0.0f, 0.0f, deltaRotation);
		Application::MatsumotoUtility::SetRotationToSprite(blockSprite_, targetRotation);
	}
}

void Application::BlockSprite::AddScale(float scaleMultiplier) {
	if (blockSprite_) {
		Vector3 currentScale = Application::MatsumotoUtility::GetScaleFromSprite(blockSprite_);
		Vector3 targetScale = currentScale * scaleMultiplier;
		Application::MatsumotoUtility::SetScaleToSprite(blockSprite_, targetScale);
	}
}
