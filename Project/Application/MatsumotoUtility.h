#pragma once
#include <KashipanEngine.h>

namespace Application::MatsumotoUtility
{
	/// @brief スプライトオブジェクトの作成
	KashipanEngine::Sprite* CreateSpriteObject(
		KashipanEngine::Window* renderWindow,
		std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase> obj)> AddObject,
		const std::string& spriteName);
	/// @brief 簡易イージング関数（イージングなしの線形補間に近い挙動）
	float SimpleEaseIn(float from, float to, float transitionSpeed);
}