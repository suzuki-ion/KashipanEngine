#pragma once
#include <KashipanEngine.h>

#include <Objects/OutGameSystem/MenuActionManager.h>
#include <Objects/Container/MenuSpriteCotainer.h>
#include <Objects/OutGameSystem/GameStartSystem.h>

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

	// * ユーティリティ * //
	// ゲームにスプライトを生成、追加する関数
	std::function<KashipanEngine::Sprite* (const std::string&)> createSpriteFunction_;
	// ゲームにスプライトを特定のテクスチャで生成、追加する関数
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteWithTextureFunction_;
	// 画面の中心
	Vector2 screenCenter_;

	// * ゲームで使うオブジェクトたち * //
	// ゲームのメインループ
	void GameLoop();
	// メニューとゲームの開始
	void InitMenu();
	Vector2 menuPosition_;
	Application::MenuActionManager menuActionManager_;
	Application::MenuSpriteContainer menuSpriteContainer_;
	Application::GameStartSystem gameStartSystem_;
	Sprite* gameStartSprite_ = nullptr;
	Sprite* gameStartGoSprite_ = nullptr;
	// ゲームオーバー
	bool gameOver_ = false;
	// ゲームオーバー後の自動シーン遷移用タイマー
	float autoSceneChangeTimer_ = 0.0f;
	// ゲームの背景
	KashipanEngine::Sprite* backgroundSprite_ = nullptr;
	// BGM
	AudioPlayer audioPlayer_;
};

} // namespace KashipanEngine