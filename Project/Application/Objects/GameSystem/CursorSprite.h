#pragma once
#include <KashipanEngine.h>

namespace Application {
	class CursorSprite {
	public:
		void Initialize(
			std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc,
			KashipanEngine::Sprite* parentSprite);
		void Update(const Vector2& cursor,bool isSelected);

		void SetPosition(const Vector3& position);

	private:
		KashipanEngine::Sprite* cursorSprite_ = nullptr;
		bool oldSelectedState_ = false;
	};
}