#include "CursorSprite.h"
#include <MatsumotoUtility.h>

void Application::CursorSprite::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc, KashipanEngine::Sprite* parentSprite) {
	cursorSprite_ = createSpriteFunc("Cursor", "Cursor_1.png", KashipanEngine::DefaultSampler::LinearClamp);
	Application::MatsumotoUtility::ParentSpriteToSprite(cursorSprite_, parentSprite);

	oldSelectedState_ = false;
}

void Application::CursorSprite::Update(const Vector2& cursor, bool isSelected) {
	if (cursorSprite_) {
		Vector3 textureScale = Application::MatsumotoUtility::GetTextureSizeFromSprite(cursorSprite_);

		Application::MatsumotoUtility::SimpleEaseSpriteMove(cursorSprite_, Vector3(cursor.x, cursor.y, 0.0f), 0.5f);
		Application::MatsumotoUtility::SimpleEaseSpriteScale(cursorSprite_, textureScale, 0.5f);
		Application::MatsumotoUtility::SimpleEaseSpriteRotate(cursorSprite_, Vector3(0.0f, 0.0f, 0.0f), 0.5f);
	}

	if (isSelected != oldSelectedState_) {
		Vector3 textureScale = Application::MatsumotoUtility::GetTextureSizeFromSprite(cursorSprite_);

		Application::MatsumotoUtility::SetScaleToSprite(cursorSprite_, textureScale*1.2f);
		Application::MatsumotoUtility::SetRotationToSprite(cursorSprite_, Vector3(0.0f,0.0f,3.14f));
		Application::MatsumotoUtility::SetTextureToSprite(cursorSprite_, isSelected ? "Cursor_2.png" : "Cursor_1.png");
		oldSelectedState_ = isSelected;
	}

}

void Application::CursorSprite::SetPosition(const Vector3& position) {
	if (cursorSprite_) {
		Application::MatsumotoUtility::SetTranslateToSprite(cursorSprite_, position);
	}
}
