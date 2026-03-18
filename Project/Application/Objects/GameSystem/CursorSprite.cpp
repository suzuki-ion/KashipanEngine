#include "CursorSprite.h"
#include <MatsumotoUtility.h>

void Application::CursorSprite::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc, KashipanEngine::Sprite* parentSprite) {
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc) {
	cursorSprite_ = createSpriteFunc("Cursor", "Cursor.png", KashipanEngine::DefaultSampler::LinearClamp);
	Application::MatsumotoUtility::ParentSpriteToSprite(cursorSprite_, parentSprite);

	oldSelectedState_ = false;
}

void Application::CursorSprite::Update(const Vector2& cursor) {
	if (cursorSprite_) {
		Application::MatsumotoUtility::SimpleEaseSpriteMove(cursorSprite_, Vector3(cursor.x, cursor.y, 0.0f), 0.5f);
	}
}

void Application::CursorSprite::Update(const Vector2& cursor, bool isSelected) {
	if (cursorSprite_) {
		Vector3 textureScale = Application::MatsumotoUtility::GetTextureSizeFromSprite(cursorSprite_);

		Application::MatsumotoUtility::SimpleEaseSpriteMove(cursorSprite_, Vector3(cursor.x, cursor.y, 0.0f), 0.5f);
		Application::MatsumotoUtility::SimpleEaseSpriteScale(cursorSprite_, textureScale, 0.5f);
		Application::MatsumotoUtility::SetRotationToSprite(cursorSprite_, Vector3(0.0f, 0.0f, 0.0f));
	}

	if (isSelected != oldSelectedState_) {
		Application::MatsumotoUtility::SetScaleToSprite(cursorSprite_, Vector3(1.2f, 1.2f, 1.2f));
		Application::MatsumotoUtility::SetRotationToSprite(cursorSprite_, Vector3(0.0f,0.0f,3.14f));
		Application::MatsumotoUtility::SetTextureToSprite(cursorSprite_, isSelected ? "Cursor_1.png" : "Cursor_2.png");
		oldSelectedState_ = isSelected;
	}

}
