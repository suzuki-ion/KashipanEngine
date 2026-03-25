#pragma once
#include <KashipanEngine.h>
#include "TitleSection.h"

namespace Application {
	// タイトル画面にあるスプライトを管理するクラス
	class TitleSpriteManager {
	public:
		void Initialize(std::function<KashipanEngine::Sprite* (const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc);

		void Update(float deltaTime, TitleSection currentSection, int selectNumber);

		void SetTriggered1PTimer(float time) { triggered1PTimer_ = time; }
		void SetTriggered2PTimer(float time) { triggered2PTimer_ = time; }

	private:
		Vector3 centerPosition_ ;

		std::map<TitleSection, std::function<void()>> sectionUpdateFunctions_;
		std::function<KashipanEngine::Sprite* (const std::string&, KashipanEngine::DefaultSampler)> CreateSpriteFunc_;
		std::map<std::string, KashipanEngine::Sprite*> sprites_;

		float timer_;
		float deltaTime_;
		TitleSection currentSection_;
		TitleSection previousSection_;
		int currentSelectNumber_;

		float triggered1PTimer_;
		float triggered2PTimer_;

	private:
		void UpdateTitleCallSection();
		void UpdateModeSelectSection();
		void UpdateAISelectSection();
		void UpdateMultiplayerSelectSection();

		// 手振れを加えるための更新関数
		void UpdateIdleSection();
	};
}
