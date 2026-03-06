#include "MatsumotoUtility.h"

KashipanEngine::Sprite* Application::MatsumotoUtility::CreateSpriteObject(
	KashipanEngine::Window* renderWindow,
	std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase>obj)> AddObject, 
	const std::string& spriteName) {

	std::unique_ptr<KashipanEngine::Sprite> sprite = std::make_unique<KashipanEngine::Sprite>();
	KashipanEngine::Sprite* spritePtr = sprite.get();
	sprite->SetName(spriteName);

	assert(AddObject);
	assert(renderWindow);

	if (renderWindow) {
		sprite->AttachToRenderer(renderWindow, "Object2D.DoubleSidedCulling.BlendNormal");
	}

	AddObject(std::move(sprite));
	return spritePtr;
}

float Application::MatsumotoUtility::SimpleEaseIn(float from, float to, float transitionSpeed)
{
	float value = from;
	value += (to - value) * transitionSpeed;
	if (fabsf(value - to) <= 0.01f) {
		return to;
	}
	return value;
}
