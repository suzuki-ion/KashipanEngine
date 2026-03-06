#pragma once
#include <KashipanEngine.h>

#include <Objects/OutGameSystem/MenuActionManager.h>
#include <Objects/Container/MenuSpriteCotainer.h>

namespace KashipanEngine {

class GameScene final : public SceneBase {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

	Vector2 menuPosition_;
	Application::MenuActionManager menuActionManager_;
	Application::MenuSpriteContainer menuSpriteContainer_;
};

} // namespace KashipanEngine