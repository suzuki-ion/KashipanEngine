#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include "Scenes/Components/GameMenuComponents.h"
#include "Scenes/Components/PuzzleBoard.h"

#include <MatsumotoUtility.h>
#include <Objects/SceneValue.h>

using namespace Application;

namespace KashipanEngine {

GameScene::GameScene()
    : SceneBase("GameScene") {}

void GameScene::Initialize() {
    sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();

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

	AddSceneComponent(std::make_unique<PuzzleBoard>());
	AddSceneComponent(std::make_unique<PuzzleBoard>());
    auto puzzleBoards = GetSceneComponents<PuzzleBoard>();
    // 盤面の初期化
	if (puzzleBoards.size() >= 2) {
		puzzleBoards[0]->SetBoardSize(17, 6);
		puzzleBoards[1]->SetBoardSize(17, 6);
		// 左右で配置
        auto boardTransform1 = puzzleBoards[0]->GetBoardRootTransform();
        auto boardTransform2 = puzzleBoards[1]->GetBoardRootTransform();
		if (boardTransform1 && boardTransform2) {
			// 盤面同士の水平距離
            float boardOffsetX = screenCenter_.x * 0.5f;
			// 盤面の垂直位置
			float boardOffsetY = 0.0f;

			boardTransform1->SetTranslate(Vector3(screenCenter_.x - boardOffsetX, screenCenter_.y + boardOffsetY, 0.0f));
            boardTransform2->SetTranslate(Vector3(screenCenter_.x + boardOffsetX, screenCenter_.y + boardOffsetY, 0.0f));
        }
    }

    // ============================================================
    // ゲームで使うオブジェクトたちの生成
    // ============================================================
	// ゲームのメニューコンポーネントを追加
	AddSceneComponent(std::make_unique<GameMenuComponents>(GetInputCommand()));
	// ゲームオーバー状態の初期化
	isGameOver_ = false;
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

	AddSceneComponent(std::make_unique<SceneChangeIn>());
	AddSceneComponent(std::make_unique<SceneChangeOut>());
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
	if (!isGameOver_ && GetSceneComponent<GameMenuComponents>()->IsCanLoop()) {
		//GameLoop();
	}

	// メニューからの遷移の処理
	if (GetSceneComponent<GameMenuComponents>()->IsRequestSceneChange()) {
		SetNextSceneName(GetSceneComponent<GameMenuComponents>()->GetNextSceneName());
		if (auto* out = GetSceneComponent<SceneChangeOut>()) {
			out->Play();
		}
	}

	// ゲームオーバー後の自動シーン遷移処理
	if (isGameOver_) {

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

} // namespace KashipanEngine
