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

	// * ゲームで使うオブジェクトたち * //
	// ゲームのメインループ
	void GameLoop();
	// パズルゲームのシステム
	Application::PuzzlePlayer puzzlePlayer1_;
	Application::PuzzleGameSystem puzzleGameSystem1_;

	Application::PuzzlePlayer puzzlePlayer2_;
	Application::AiPlayer aiPlayer2_;
	Application::PuzzleGameSystem puzzleGameSystem2_;

	// ゲームオーバー
	bool gameOver_ = false;
	// ゲームオーバー後の自動シーン遷移用タイマー
	float autoSceneChangeTimer_ = 0.0f;
	// ゲームの背景
	KashipanEngine::Sprite* backgroundSprite_ = nullptr;
	// NPCモードかどうか
	bool isNpcMode_ = false; 
	// ゲームの操作チュートリアル
	Sprite* tutorialSprite_ = nullptr;
	Sprite* trSlide1p = nullptr;
	Sprite* trSlide2p = nullptr;
	Sprite* trAttack1p = nullptr;
	Sprite* trAttack2p = nullptr;
	float trSlideTimer1P_ = 0.0f;
	float trSlideTimer2P_ = 0.0f;
	float trAttackTimer1P_ = 0.0f;
	float trAttackTimer2P_ = 0.0f;

	// BGM
	AudioPlayer audioPlayer_;
};

} // namespace KashipanEngine