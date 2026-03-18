#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

#include <MatsumotoUtility.h>
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
	puzzleGameSystem_.Initialize(createSpriteWithTextureFunction_, &puzzlePlayer1_);
	puzzleGameSystem_.SetAnchorSpritePosition(Vector3(screenCenter_.x * 0.5f, screenCenter_.y, 0.0f));
	
	// メニューのスプライトを初期化
    menuSpriteContainer_.Initialize([this](const std::string& name, const std::string& textureName) {
        return createSpriteWithTextureFunction_(name, textureName, KashipanEngine::DefaultSampler::LinearClamp);
    });
    menuPosition_ = Vector2(400.0f, 250.0f);
    menuSpriteContainer_.SetPosition(menuPosition_);

	// ゲーム開始のスプライトを初期化
    gameStartSprite_ = createSpriteWithTextureFunction_("GameStart", "GameStart.png", KashipanEngine::DefaultSampler::LinearClamp);
    gameStartGoSprite_ = createSpriteWithTextureFunction_("GameStartGo", "GameStartGo.png", KashipanEngine::DefaultSampler::LinearClamp);
	Application::MatsumotoUtility::SetTranslateToSprite(gameStartSprite_, Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
	Application::MatsumotoUtility::SetTranslateToSprite(gameStartGoSprite_, Vector3(screenCenter_.x, screenCenter_.y, 0.0f));
	Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, Vector3(0.0f, 0.0f, 1.0f));

    // ============================================================
    // ゲームで使うオブジェクトたちの生成
    // ============================================================
	// 3.0秒後にゲーム開始のシーケンスを開始するオブジェクト
	gameStartSystem_.Initialize();
	gameStartSystem_.SetStartDelay(3.0f);
	// メニューで使う入力、アクションの追加＆処理
	InitMenu();
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

	// メニューの位置を更新
	menuSpriteContainer_.SetPosition(menuPosition_);
	// ゲーム開始前はメニューを開けないようにする
	if (gameStartSystem_.IsGameStarted()) {
		menuActionManager_.Update();
	}
	// メニューのスプライトの状態を更新
	menuSpriteContainer_.SetSelectedIndex(menuActionManager_.GetSelectedIndex());
	menuSpriteContainer_.SetMenuOpen(menuActionManager_.IsMenuOpen());
	menuSpriteContainer_.Update(deltaTime);

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

	// ゲーム開始のシーケンスを更新
	gameStartSystem_.Update(deltaTime);
	// ゲームオーバーでない、かつメニューが開いていない、かつゲーム開始のシーケンスが終わっている場合はゲームのメインループを回す
	if (!gameOver_ && !menuActionManager_.IsMenuOpen() && gameStartSystem_.IsGameStarted()) {
		GameLoop();
	}

	// ゲーム開始前のスプライトモーション
	if (!gameStartSystem_.IsGameStarted()) {
		Application::MatsumotoUtility::RotateSprite(gameStartSprite_, Vector3(0.0f, 3.14f * deltaTime, 0.0f));
	}

	// ゲームオーバー後の自動シーン遷移処理
	if (gameOver_) {
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

	ImGui::Begin("GameScene");
	if (ImGui::Button("NextScene")) {
		if (auto* out = GetSceneComponent<SceneChangeOut>()) {
			out->Play();
		}
	}
	ImGui::End();
}

// ゲームのメインループ
void GameScene::GameLoop()
{
	// ゲーム開始演出の更新
	Vector3 startGameSpriteScale = Application::MatsumotoUtility::GetScaleFromSprite(gameStartSprite_);
	startGameSpriteScale.x = Application::MatsumotoUtility::SimpleEaseIn(startGameSpriteScale.x, 0.0f, 0.3f);
	startGameSpriteScale.y = Application::MatsumotoUtility::SimpleEaseIn(startGameSpriteScale.y, 0.0f, 0.3f);
	Application::MatsumotoUtility::SetScaleToSprite(gameStartSprite_, startGameSpriteScale);
	// GOスプライトは透明にしながら拡大させる
	Vector3 goSpriteScale = Application::MatsumotoUtility::GetTextureSizeFromSprite(gameStartGoSprite_);
	Vector4 currentColor = Application::MatsumotoUtility::GetColorFromSprite(gameStartGoSprite_);
	Vector3 currentScale = Application::MatsumotoUtility::GetScaleFromSprite(gameStartGoSprite_);
	currentColor.w = Application::MatsumotoUtility::SimpleEaseIn(currentColor.w, 0.0f, 0.1f);
	currentScale.x = Application::MatsumotoUtility::SimpleEaseIn(currentScale.x, goSpriteScale.x, 0.3f);
	currentScale.y = Application::MatsumotoUtility::SimpleEaseIn(currentScale.y, goSpriteScale.y, 0.3f);
	Application::MatsumotoUtility::SetColorToSprite(gameStartGoSprite_, currentColor);
	Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, currentScale);

	// パズルゲームのシステムの更新
	puzzlePlayer1_.Update(KashipanEngine::GetDeltaTime());
	puzzleGameSystem_.Update();
}

// メニューで使う入力、アクションの追加＆処理
void GameScene::InitMenu()
{
    menuActionManager_.Initialize(
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Menu").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Submit").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Cancel").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Down").Triggered(); },
        [this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Up").Triggered(); }
    );
    // メニューのアクションを追加
    menuActionManager_.AddMenuAction([this]() {
        gameStartSystem_.Initialize();
        gameStartSystem_.StartSequence(1.5f);
        Application::MatsumotoUtility::SetColorToSprite(gameStartSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        Application::MatsumotoUtility::FitSpriteToTexture(gameStartSprite_);
        Application::MatsumotoUtility::SetColorToSprite(gameStartGoSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
        Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, Vector3(0.0f, 0.0f, 1.0f));
        menuActionManager_.SetMenuOpen(false);
        });
	menuActionManager_.AddMenuAction([this]() {
		if (auto* out = GetSceneComponent<SceneChangeOut>()) {
			SetNextSceneName("GameScene");
			out->Play();
		}
		});
    menuActionManager_.AddMenuAction([this]() {
        if (auto* out = GetSceneComponent<SceneChangeOut>()) {
            SetNextSceneName("TitleScene");
            out->Play();
        }
        });

	// メニューでそうさUI用の関数登録
	ControllerViewer* controllerViewer = menuSpriteContainer_.GetControllerViewer();
	controllerViewer->SetInputCheckFunction("move", [this]() {
		auto* ic = GetInputCommand();
		return ic && (ic->Evaluate("Up").Triggered() || ic->Evaluate("Down").Triggered() || ic->Evaluate("Left").Triggered() || ic->Evaluate("Right").Triggered());
		});
	controllerViewer->SetInputCheckFunction("a", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerA").Triggered();
		});
	controllerViewer->SetInputCheckFunction("b", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerB").Triggered();
		});
	controllerViewer->SetInputCheckFunction("x", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerX").Triggered();
		});
	controllerViewer->SetInputCheckFunction("y", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerY").Triggered();
		});
	controllerViewer->SetInputCheckFunction("lt", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerLeftShoulder").Triggered();
		});
	controllerViewer->SetInputCheckFunction("rt", [this]() {
		auto* ic = GetInputCommand();
		return ic && ic->Evaluate("ControllerRightShoulder").Triggered();
		});
}

} // namespace KashipanEngine
