#pragma once
#include <KashipanEngine.h>
#include "TitleSection.h"

namespace Application {
	// タイトル画面にあるスプライトを管理するクラス
	class TitleSpriteManager {
	public:
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&)> createSpriteFunc);

		void Update(float deltaTime, TitleSection currentSection, int selectNumber);

	private:
		Vector3 centerPosition_ ;

		std::map<TitleSection, std::function<void()>> sectionUpdateFunctions_;
		std::function<KashipanEngine::Sprite* (const std::string&)> CreateSpriteFunc_;
		std::map<std::string, KashipanEngine::Sprite*> sprites_;

		float deltaTime_;
		TitleSection currentSection_;
		int currentSelectNumber_;

	private:
		void UpdateTitleCallSection();
		void UpdateModeSelectSection();
		void UpdateAISelectSection();
		void UpdateMultiplayerSelectSection();
	};
}
