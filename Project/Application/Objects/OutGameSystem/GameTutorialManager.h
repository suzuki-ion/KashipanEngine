#pragma once
#include <KashipanEngine.h>

namespace Application {
	/// ゲームの操作チュートリアルを管理するクラス
	class GameTutorialManager {
	public:
		void Initialize(std::vector<KashipanEngine::Sprite*> tutorialSprite);
		void Update();

		void SetPosition(const Vector2& position) { tutorialPosition_ = position; }
		void SetIsGrip(bool isGrip) { isGrip_ = isGrip; }
		void SetIsActiveTutorial(bool isActive) { isTutorialActive_ = isActive; }

	private:
		void SpriteVisible(bool visible);

		Vector2 tutorialPosition_;
		bool isTutorialActive_;
		bool isPlayer_;

		bool isGrip_;

		std::vector<KashipanEngine::Sprite*> tutorialSprites_;
		std::vector<Vector2> tutorialSpriteOffsets_;
	};
} // namespace Application