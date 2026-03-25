#pragma once
#include <KashipanEngine.h>
#include "Objects/OutGameSystem/MenuActionManager.h"
#include "Objects/OutGameSystem/MenuSpriteCotainer.h"
#include "Objects/OutGameSystem/GameStartSystem.h"

namespace Application {
    class GameMenuComponents : public KashipanEngine::ISceneComponent {
    public:
        GameMenuComponents(KashipanEngine::InputCommand* ic)
            : ISceneComponent("GameMenuComponents", 1),ic_(ic) {}
        ~GameMenuComponents() override = default;

        void Initialize() override;
        void Update() override;

		// ゲームのメインループに入れるかどうか
        bool IsCanLoop() const {
            return  !menuActionManager_.IsMenuOpen() && gameStartSystem_.IsGameStarted();}
		// シーンの遷移を要求しているかどうか
		bool IsRequestSceneChange() const { return isRequestSceneChange_; }
		// 遷移先のシーン名を取得する
		const std::string& GetNextSceneName() const { return nextSceneName_; }

    private:
        KashipanEngine::InputCommand* ic_;
        MenuActionManager menuActionManager_;
		MenuSpriteContainer menuSpriteContainer_;

        Application::GameStartSystem gameStartSystem_;
        KashipanEngine::Sprite* gameStartSprite_ = nullptr;
        KashipanEngine::Sprite* gameStartGoSprite_ = nullptr;

		bool isRequestSceneChange_ = false;
		std::string nextSceneName_;

        Vector2 menuPosition_;
    };

}