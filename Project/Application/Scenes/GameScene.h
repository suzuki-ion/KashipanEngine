#pragma once
#include <KashipanEngine.h>

#include <Objects/OutGameSystem/MenuActionManager.h>
#include <Objects/OutGameSystem/MenuSpriteCotainer.h>
#include <Objects/OutGameSystem/GameStartSystem.h>

#include <Objects/GameSystem/PuzzleGameSystem.h>
#include <Objects/GameSystem/AiPlayer.h>

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
	std::function<KashipanEngine::Sprite* (const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunction_;
	// ゲームにスプライトを特定のテクスチャで生成、追加する関数
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteWithTextureFunction_;
	// 画面の中心
	Vector2 screenCenter_;

	// ゲームオーバー
	bool isGameOver_ = false;
	// ゲームオーバー後の自動シーン遷移用タイマー
	float autoSceneChangeTimer_ = 0.0f;
	// ゲームの背景
	KashipanEngine::Sprite* backgroundSprite_ = nullptr;
	// NPCモードかどうか
	bool isNpcMode_ = false; 

	// BGM
	AudioPlayer audioPlayer_;
};

} // namespace KashipanEngine