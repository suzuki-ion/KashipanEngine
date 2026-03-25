#pragma once
#include <KashipanEngine.h>

namespace Application {
	class GaugeSprite {
	public:
		void Initialize(std::function < KashipanEngine::Sprite* (const std::string&,
			const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc);
		void Update(float rate,float fillMaxRate = 1.0f);
		void SetSize(const Vector3& size);

		void SetPosition(const Vector3& position);
		void SetRotation(const Vector3& rotation);

		void SetParent(KashipanEngine::Sprite* parent);

	private:
		KashipanEngine::Sprite* anchorSprite_ = nullptr;

		KashipanEngine::Sprite* gaugeSprite_ = nullptr;
		KashipanEngine::Sprite* gaugeFillSprite_ = nullptr;
		KashipanEngine::Sprite* gaugeAnimationSprite_ = nullptr;
		KashipanEngine::Sprite* gaugeFillAnimationSprite_ = nullptr;
		Vector3 size_ = Vector3(100.0f, 20.0f, 1.0f);


	};
}
