#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include "Scenes/Components/GameMenuComponents.h"

#include <MatsumotoUtility.h>
#include <Objects/SceneValue.h>

using namespace Application;

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

    AddSceneComponent(std::make_unique<SceneChangeIn>());
    AddSceneComponent(std::make_unique<SceneChangeOut>());

    if (auto *in = GetSceneComponent<SceneChangeIn>()) {
        in->Play();
    }

	// リザルトシーンへの遷移をセット
	SetNextSceneName("ResultScene");
	isNpcMode_ = Application::Value::isNpcMode; // NPCモードの設定を取得

    // ============================================================
    // ユーティリティの生成
    // ============================================================
	// ゲームにスプライトを生成、追加する関数
    createSpriteFunction_ =
        [this](const std::string& name, KashipanEngine::DefaultSampler defaultSampler) {
            return Application::MatsumotoUtility::CreateSpriteObject(
                sceneDefaultVariables_->GetScreenBuffer2D(),
                [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); },
                name,
                defaultSampler);
		};
	// ゲームにスプライトを特定のテクスチャで生成、追加する関数
    createSpriteWithTextureFunction_ =
        [this](const std::string& name, const std::string& textureName, KashipanEngine::DefaultSampler defaultSampler) {
        KashipanEngine::Sprite* sprite = createSpriteFunction_(name, defaultSampler);
        Application::MatsumotoUtility::SetTextureToSprite(sprite, textureName);
		Application::MatsumotoUtility::FitSpriteToTexture(sprite);
        return sprite;
        };
	// 画面の中心
	if (auto* screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
		screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
	}

    // ============================================================
    // ゲームで使うスプライトたちの生成
    // ============================================================
	// ゲームの背景スプライトを初期化
	backgroundSprite_ = createSpriteWithTextureFunction_("Background", "TitleBG.png", KashipanEngine::DefaultSampler::LinearWrap);
	Application::MatsumotoUtility::SetTranslateToSprite(backgroundSprite_, Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
	
	// パズルゲームのシステムの初期化
	puzzlePlayer1_.Initialize();
	puzzlePlayer1_.SetMoveUpFunction([this]() { return GetInputCommand()->Evaluate("PuzzleUp").Triggered(); });
	puzzlePlayer1_.SetMoveDownFunction([this]() { return GetInputCommand()->Evaluate("PuzzleDown").Triggered(); });
	puzzlePlayer1_.SetMoveLeftFunction([this]() { return GetInputCommand()->Evaluate("PuzzleLeft").Triggered(); });
	puzzlePlayer1_.SetMoveRightFunction([this]() { return GetInputCommand()->Evaluate("PuzzleRight").Triggered(); });
	puzzlePlayer1_.SetSelectFunction([this]() { return GetInputCommand()->Evaluate("PuzzleActionHold").Triggered(); });
	puzzlePlayer1_.SetSendFunction([this]() { return GetInputCommand()->Evaluate("PuzzleTimeSkip").Triggered(); });
	puzzleGameSystem1_.Initialize(createSpriteWithTextureFunction_, &puzzlePlayer1_);
	puzzleGameSystem1_.SetAnchorSpritePosition(Vector3(screenCenter_.x * 0.5f, screenCenter_.y, 0.0f));
	
	puzzlePlayer2_.Initialize();
	aiPlayer2_.Initialize(Application::Value::npcNumber);
	// 2P用のパズルゲームのシステムの初期化
	if (isNpcMode_) {
		puzzlePlayer2_.SetMoveUpFunction([this]() { return aiPlayer2_.GetIsMoveUp(); });
		puzzlePlayer2_.SetMoveDownFunction([this]() { return aiPlayer2_.GetIsMoveDown(); });
		puzzlePlayer2_.SetMoveLeftFunction([this]() { return aiPlayer2_.GetIsMoveLeft(); });
		puzzlePlayer2_.SetMoveRightFunction([this]() { return aiPlayer2_.GetIsMoveRight(); });
		puzzlePlayer2_.SetSelectFunction([this]() { return aiPlayer2_.GetIsSelecting(); });
		puzzlePlayer2_.SetSendFunction([this]() { return aiPlayer2_.GetIsSend(); });
	}
	else {
		puzzlePlayer2_.SetMoveUpFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleUp").Triggered(); });
		puzzlePlayer2_.SetMoveDownFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleDown").Triggered(); });
		puzzlePlayer2_.SetMoveLeftFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleLeft").Triggered(); });
		puzzlePlayer2_.SetMoveRightFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleRight").Triggered(); });
		puzzlePlayer2_.SetSelectFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleActionHold").Triggered(); });
		puzzlePlayer2_.SetSendFunction([this]() { return GetInputCommand()->Evaluate("P2PuzzleTimeSkip").Triggered(); });
	}
	
	puzzleGameSystem2_.Initialize(createSpriteWithTextureFunction_, &puzzlePlayer2_);
	puzzleGameSystem2_.SetAnchorSpritePosition(Vector3(screenCenter_.x * 1.5f, screenCenter_.y, 0.0f));
	puzzleGameSystem2_.SetAnchorSpriteRotation(Vector3(0.0f, 3.14f, 0.0f)); // 2P側の盤面を反転させる
	if (isNpcMode_) {
		puzzleGameSystem2_.SetupNpc();
	}
	else {
		puzzleGameSystem2_.Setup2P();
	}
	
	// 
	tutorialSprite_ = createSpriteWithTextureFunction_("Tutorial", "ControllTutorial.png", KashipanEngine::DefaultSampler::LinearClamp);
	MatsumotoUtility::SetTranslateToSprite(tutorialSprite_, Vector3(screenCenter_.x, screenCenter_.y - 100.0f, 0.0f));

	trSlide1p = createSpriteWithTextureFunction_("CR_Slide_1p", "CR_Slide_1p.png", KashipanEngine::DefaultSampler::LinearClamp);
	trSlide2p = createSpriteWithTextureFunction_("CR_Slide_2p", "CR_Slide_2p.png", KashipanEngine::DefaultSampler::LinearClamp);
	trAttack1p = createSpriteWithTextureFunction_("CR_Attack_1p", "CR_Attack_1p.png", KashipanEngine::DefaultSampler::LinearClamp);
	trAttack2p = createSpriteWithTextureFunction_("CR_Attack_2p", "CR_Attack_2p.png", KashipanEngine::DefaultSampler::LinearClamp);
	MatsumotoUtility::SetTranslateToSprite(trSlide1p, Vector3(screenCenter_.x, screenCenter_.y - 100.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(trSlide2p, Vector3(screenCenter_.x, screenCenter_.y - 100.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(trAttack1p, Vector3(screenCenter_.x, screenCenter_.y - 100.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(trAttack2p, Vector3(screenCenter_.x, screenCenter_.y - 100.0f, 0.0f));
	MatsumotoUtility::SetColorToSprite(trSlide1p, Vector4(1.0f, 1.0f, 1.0f, 0.0f));
	MatsumotoUtility::SetColorToSprite(trSlide2p, Vector4(1.0f, 1.0f, 1.0f, 0.0f));
	MatsumotoUtility::SetColorToSprite(trAttack1p, Vector4(1.0f, 1.0f, 1.0f, 0.0f));
	MatsumotoUtility::SetColorToSprite(trAttack2p, Vector4(1.0f, 1.0f, 1.0f, 0.0f));
	trSlideTimer1P_ = 0.0f;
	trSlideTimer2P_ = 0.0f;
	trAttackTimer1P_ = 0.0f;
	trAttackTimer2P_ = 0.0f;

    // ============================================================
    // ゲームで使うオブジェクトたちの生成
    // ============================================================
	// ゲームのメニューコンポーネントを追加
	AddSceneComponent(std::make_unique<GameMenuComponents>(GetInputCommand()));
	// ゲームオーバー状態の初期化
	gameOver_ = false;
	// ゲームオーバー後の遷移までの時間を初期化
	autoSceneChangeTimer_ = 3.0f;

	// ================================================================
	// BGM
	// ================================================================
	{
		AudioManager::PlayParams params;
		params.sound = AudioManager::GetSoundHandleFromFileName("bgmGame.mp3");
		params.volume = 0.2f;
		params.loop = true;
		audioPlayer_.AddAudio(params);
		audioPlayer_.ChangeAudio(2.0);
	}
}

GameScene::~GameScene() {}

void GameScene::OnUpdate() {
	// 画面の中心を更新
	if (auto *screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
		screenCenter_ = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
	}
	// デルタタイムの取得
	float deltaTime = GetDeltaTime();

	// シーン遷移処理
	if (auto* ic = GetInputCommand()) {
		if (ic->Evaluate("DebugSceneChange").Triggered()) {
			if (GetNextSceneName().empty()) {
				SetNextSceneName("MenuScene");
			}
			if (auto* out = GetSceneComponent<SceneChangeOut>()) {
				out->Play();
			}
		}
	}
	// シーン遷移のアニメーションが終わったら次のシーンへ
	if (!GetNextSceneName().empty()) {
		if (auto* out = GetSceneComponent<SceneChangeOut>()) {
			if (out->IsFinished()) {
				ChangeToNextScene();
			}
		}
	}

	// 背景画像の更新
	Application::MatsumotoUtility::MoveTextureUVToSprite(backgroundSprite_, Vector2(0.0f, GetDeltaTime()));

	
	// ゲームオーバーでない、かつメニューが開いていない、かつゲーム開始のシーケンスが終わっている場合はゲームのメインループを回す
	if (!gameOver_ && GetSceneComponent<GameMenuComponents>()->IsCanLoop()) {
		GameLoop();
	}

	// メニューからの遷移の処理
	if (GetSceneComponent<GameMenuComponents>()->IsRequestSceneChange()) {
		SetNextSceneName(GetSceneComponent<GameMenuComponents>()->GetNextSceneName());
		if (auto* out = GetSceneComponent<SceneChangeOut>()) {
			out->Play();
		}
	}

	// ゲームオーバー後の自動シーン遷移処理
	if (gameOver_) {
		puzzleGameSystem1_.DeathAnimation();
		puzzleGameSystem2_.DeathAnimation();

		autoSceneChangeTimer_ -= deltaTime;
		if (autoSceneChangeTimer_ <= 0.0f) {
			if (GetNextSceneName().empty()) {
				SetNextSceneName("MenuScene");
			}
			if (auto* out = GetSceneComponent<SceneChangeOut>()) {
				out->Play();
			}
		}
	}
}

// ゲームのメインループ
void GameScene::GameLoop()
{
	if(!puzzlePlayer1_.IsAlive() || !puzzlePlayer2_.IsAlive()) {
		gameOver_ = true;
		Application::Value::winnerPlayerNumber = puzzlePlayer1_.IsAlive() ? 0 : 1;
		Application::Value::isNpcMode = isNpcMode_;
		return;
	}

	// パズルゲームのシステムの更新
	puzzlePlayer1_.Update(KashipanEngine::GetDeltaTime());
	puzzleGameSystem1_.Update();

	if (isNpcMode_) {
		aiPlayer2_.Update(KashipanEngine::GetDeltaTime());
	}
	puzzlePlayer2_.Update(KashipanEngine::GetDeltaTime());
	puzzleGameSystem2_.Update();

	puzzleGameSystem1_.TakeDamage(puzzleGameSystem2_.SendDamage());
	puzzleGameSystem2_.TakeDamage(puzzleGameSystem1_.SendDamage());

	// チュートリアルの表示
	if (puzzlePlayer1_.IsSelect() &&
		(puzzlePlayer1_.IsMoveUp() || puzzlePlayer1_.IsMoveDown() || puzzlePlayer1_.IsMoveLeft() || puzzlePlayer1_.IsMoveRight())) {
		trSlideTimer1P_ = 1.0f;
	}
	if (puzzlePlayer1_.IsSend()) {
		trAttackTimer1P_ = 1.0f;
	}
	if(puzzlePlayer2_.IsSelect() &&
		(puzzlePlayer2_.IsMoveUp() || puzzlePlayer2_.IsMoveDown() || puzzlePlayer2_.IsMoveLeft() || puzzlePlayer2_.IsMoveRight())) {
		trSlideTimer2P_ = 1.0f;
	}
	if(puzzlePlayer2_.IsSend()) {
		trAttackTimer2P_ = 1.0f;
	}

	MatsumotoUtility::SetColorToSprite(trSlide1p, Vector4(1.0f, 1.0f, 1.0f, trSlideTimer1P_));
	MatsumotoUtility::SetColorToSprite(trSlide2p, Vector4(1.0f, 1.0f, 1.0f, trSlideTimer2P_));
	MatsumotoUtility::SetColorToSprite(trAttack1p, Vector4(1.0f, 1.0f, 1.0f, trAttackTimer1P_));
	MatsumotoUtility::SetColorToSprite(trAttack2p, Vector4(1.0f, 1.0f, 1.0f, trAttackTimer2P_));

	trSlideTimer1P_ = MatsumotoUtility::SimpleEaseIn(trSlideTimer1P_, 0.0f, 0.1f);
	trSlideTimer2P_ = MatsumotoUtility::SimpleEaseIn(trSlideTimer2P_, 0.0f, 0.1f);
	trAttackTimer1P_ = MatsumotoUtility::SimpleEaseIn(trAttackTimer1P_, 0.0f, 0.1f);
	trAttackTimer2P_ = MatsumotoUtility::SimpleEaseIn(trAttackTimer2P_, 0.0f, 0.1f);
}

} // namespace KashipanEngine
