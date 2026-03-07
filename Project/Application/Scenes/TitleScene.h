#pragma once
#include <KashipanEngine.h>
#include <Objects/TitleSystem/TitleSelectManager.h>
#include <Objects/TitleSystem/TitleSpriteManager.h>

namespace KashipanEngine {

class TitleScene final : public SceneBase {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;

protected:
    void OnUpdate() override;

private:
    SceneDefaultVariables *sceneDefaultVariables_ = nullptr;

	// タイトル画面のセクション管理(データのみ)
	Application::TitleSelectManager titleSelectManager_;
	// タイトル画面のスプライト管理
	Application::TitleSpriteManager titleSpriteManager_;
};

} // namespace KashipanEngine