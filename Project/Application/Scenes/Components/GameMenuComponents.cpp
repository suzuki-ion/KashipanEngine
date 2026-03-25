#include "GameMenuComponents.h"
#include <MatsumotoUtility.h> 

#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"

using namespace Application;
using namespace KashipanEngine;

void Application::GameMenuComponents::Initialize() {
	SceneContext* context = GetOwnerContext();
	// スプライト生成関数の定義
	auto createSpriteFunction = [this,context](const std::string& name,const std::string& textureName, DefaultSampler defaultSampler) {
		return Application::MatsumotoUtility::CreateSpriteObjectWithTextureFromSceneContext(context, name,textureName, defaultSampler);
		};
	// 画面の中心を取得
	Vector2 screenCenter(0.0f, 0.0f);
	if (auto* screenBuffer2D = Application::MatsumotoUtility::GetWindowScreenBufferFromSceneContext(context)) {
		screenCenter = Vector2(static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f, static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
	}

	// メニューのスプライトを初期化
	menuSpriteContainer_.Initialize([this, createSpriteFunction](const std::string& name, const std::string& textureName) {
		return createSpriteFunction(name, textureName, KashipanEngine::DefaultSampler::LinearClamp);
		});
	menuPosition_ = Vector2(400.0f, 250.0f);
	menuSpriteContainer_.SetPosition(menuPosition_);

	// ゲーム開始のスプライトを初期化
	gameStartSprite_ = createSpriteFunction("GameStart", "GameStart.png", KashipanEngine::DefaultSampler::LinearClamp);
	gameStartGoSprite_ = createSpriteFunction("GameStartGo", "GameStartGo.png", KashipanEngine::DefaultSampler::LinearClamp);
	Application::MatsumotoUtility::SetTranslateToSprite(gameStartSprite_, Vector3(screenCenter.x, screenCenter.y, 0.0f));
	Application::MatsumotoUtility::SetTranslateToSprite(gameStartGoSprite_, Vector3(screenCenter.x, screenCenter.y, 0.0f));
	Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, Vector3(0.0f, 0.0f, 1.0f));

	menuActionManager_.Initialize(
		[this]() { return ic_ && ic_->Evaluate("Menu").Triggered(); },
		[this]() { return ic_ && ic_->Evaluate("Submit").Triggered(); },
		[this]() { return ic_ && ic_->Evaluate("Cancel").Triggered(); },
		[this]() { return ic_ && ic_->Evaluate("Down").Triggered(); },
		[this]() { return ic_ && ic_->Evaluate("Up").Triggered(); }
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
		if (isRequestSceneChange_) return;
		isRequestSceneChange_ = true;
		nextSceneName_ = "GameScene";
		});
	menuActionManager_.AddMenuAction([this]() {
		if (isRequestSceneChange_) return;
		isRequestSceneChange_ = true;
		nextSceneName_ = "TitleScene";
		});

	// メニューでそうさUI用の関数登録
	ControllerViewer* controllerViewer = menuSpriteContainer_.GetControllerViewer();
	controllerViewer->SetInputCheckFunction("move", [this]() {
		return ic_ && (ic_->Evaluate("Up").Triggered() ||
			ic_->Evaluate("Down").Triggered() ||
			ic_->Evaluate("Left").Triggered() ||
			ic_->Evaluate("Right").Triggered());
		});
	controllerViewer->SetInputCheckFunction("a", [this]() {
		return ic_ && ic_->Evaluate("ControllerA").Triggered();
		});
	controllerViewer->SetInputCheckFunction("b", [this]() {
		return ic_ && ic_->Evaluate("ControllerB").Triggered();
		});
	controllerViewer->SetInputCheckFunction("x", [this]() {
		return ic_ && ic_->Evaluate("ControllerX").Triggered();
		});
	controllerViewer->SetInputCheckFunction("y", [this]() {
		return ic_ && ic_->Evaluate("ControllerY").Triggered();
		});
	controllerViewer->SetInputCheckFunction("lt", [this]() {
		return ic_ && ic_->Evaluate("ControllerLeftShoulder").Triggered();
		});
	controllerViewer->SetInputCheckFunction("rt", [this]() {
		return ic_ && ic_->Evaluate("ControllerRightShoulder").Triggered();
		});

	// 3.0秒後にゲーム開始のシーケンスを開始するオブジェクト
	gameStartSystem_.Initialize();
	gameStartSystem_.SetStartDelay(3.0f);
}

void Application::GameMenuComponents::Update() {
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

	// ゲーム開始のシーケンスを更新
	gameStartSystem_.Update(deltaTime);

	// ゲーム開始前のスプライトモーション
	if (!gameStartSystem_.IsGameStarted()) {
		Application::MatsumotoUtility::RotateSprite(gameStartSprite_, Vector3(0.0f, 3.14f * deltaTime, 0.0f));
	}

	// ゲーム開始のシーケンスが始まっていたら、ゲーム開始の演出を更新
	if (IsCanLoop()) {
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
	}
}
