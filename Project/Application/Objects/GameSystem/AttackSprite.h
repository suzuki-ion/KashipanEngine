#pragma once
#include <KashipanEngine.h>

namespace Application {
	class AttackSprite {
	public:
		/// @brief 攻撃演出のスプライトを初期化する
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc);
		/// @brief 攻撃演出の状態を更新する
		void Update();

		void SetParent(KashipanEngine::Sprite* parent);

		void PlayAttackAnimation();

	private:
		float animationTimer_ = 0.0f;

		Vector3 velocity_ = Vector3(0.0f, 0.0f, 0.0f);

		KashipanEngine::Sprite* attackSprite_ = nullptr;
		KashipanEngine::Sprite* noiseSprite_ = nullptr;
		KashipanEngine::Sprite* playerSprite_ = nullptr;
	};
}
