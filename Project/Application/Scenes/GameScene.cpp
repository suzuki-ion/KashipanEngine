#include "Scenes/GameScene.h"
#include "Scenes/Components/SceneChangeIn.h"
#include "Scenes/Components/SceneChangeOut.h"
#include "Objects/TitleSystem/TitleSection.h"

#include <algorithm>
#include <MatsumotoUtility.h>

namespace KashipanEngine {

GameScene::GameScene()
	: SceneBase("GameScene") {
}

GameScene::~GameScene() {}

void GameScene::Initialize() {
	SetNextSceneName("ResultScene");
	auto titleSection = GetSceneVariableOr<Application::TitleSection>("SelectedModeSection", Application::TitleSection::AISelect);
	int aiDifficultyInt = GetSceneVariableOr<int>("SelectedAINumber", -1);
	gameStartSystem_.Initialize();
	gameStartSystem_.StartSequence(3.0f);
	sceneDefaultVariables_ = GetSceneComponent<SceneDefaultVariables>();
	AddSceneComponent(std::make_unique<SceneChangeIn>());
	AddSceneComponent(std::make_unique<SceneChangeOut>());
	if (auto* in = GetSceneComponent<SceneChangeIn>()) { in->Play(); }
	config_.LoadFromJSON(kConfigPath);
	auto* screenBuffer2D = sceneDefaultVariables_ ? sceneDefaultVariables_->GetScreenBuffer2D() : nullptr;
	auto* window = Window::GetWindow("3104_Noisend");
	screenCenter_ = Vector2(
		screenBuffer2D ? static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f : 960.0f,
		screenBuffer2D ? static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f : 540.0f);
	float cx = screenCenter_.x;
	float cy = screenCenter_.y;
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->SetUniqueBatchKey();
		sprite->SetName("PuzzleBackground");
		sprite->SetPivotPoint(0.5f, 0.5f);
		if (auto* mat = sprite->GetComponent2D<Material2D>()) {
			mat->SetTexture(TextureManager::GetTextureFromFileName("puzzleBackground.png"));
			mat->SetColor(Vector4(0.5f, 0.5f, 0.5f, 1.0f));
		}
		if (auto* tr = sprite->GetComponent2D<Transform2D>()) {
			tr->SetTranslate(Vector3(cx, cy, 0.0f));
			float w = window ? static_cast<float>(window->GetClientWidth()) : 1920.0f;
			float h = window ? static_cast<float>(window->GetClientHeight()) : 1080.0f;
			tr->SetScale(Vector3(w, h, 1.0f));
		}
		if (screenBuffer2D) sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
		else if (window) sprite->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
		backgroundSprite_ = sprite.get();
		AddObject2D(std::move(sprite));
	}
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->SetName("P1_Parent");
		sprite->SetPivotPoint(0.5f, 0.5f);
		if (auto* tr = sprite->GetComponent2D<Transform2D>()) tr->SetTranslate(Vector3(cx * 0.5f, cy, 0.0f));
		player1ParentSprite_ = sprite.get();
		player1ParentTransform_ = sprite->GetComponent2D<Transform2D>();
		AddObject2D(std::move(sprite));
	}
	{
		auto sprite = std::make_unique<Sprite>();
		sprite->SetName("P2_Parent");
		sprite->SetPivotPoint(0.5f, 0.5f);
		if (auto* tr = sprite->GetComponent2D<Transform2D>()) tr->SetTranslate(Vector3(cx * 1.5f, cy, 0.0f));
		player2ParentSprite_ = sprite.get();
		player2ParentTransform_ = sprite->GetComponent2D<Transform2D>();
		AddObject2D(std::move(sprite));
	}
	auto addObj2D = [this](std::unique_ptr<Object2DBase> obj) { return AddObject2D(std::move(obj)); };
	player1_.Initialize(config_, screenBuffer2D, window, addObj2D, player1ParentTransform_, "P1", "Puzzle", false);
	player2_.Initialize(config_, screenBuffer2D, window, addObj2D, player2ParentTransform_, "P2", "P2Puzzle", true);
	isNPCMode_ = (titleSection == Application::TitleSection::AISelect);
	if (isNPCMode_) {
		npcDifficulty_ = static_cast<Application::PuzzleNPC::Difficulty>(std::clamp(aiDifficultyInt, 0, 2));
		npc_.Initialize(&player2_, npcDifficulty_);
	}
	{
		auto text = std::make_unique<Text>(64);
		if (auto* tr = text->GetComponent2D<Transform2D>()) tr->SetTranslate(Vector3(cx, cy * 1.8f, 0.0f));
		text->SetFont("Assets/Application/test.fnt");
		text->SetText("");
		text->SetTextAlign(TextAlignX::Center, TextAlignY::Center);
		if (screenBuffer2D) text->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
		else if (window) text->AttachToRenderer(window, "Object2D.DoubleSidedCulling.BlendNormal");
		resultText_ = text.get();
		AddObject2D(std::move(text));
	}
	gameOver_ = false;
	winner_ = 0;
	menuActionManager_.Initialize(
		[this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Menu").Triggered(); },
		[this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Submit").Triggered(); },
		[this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Cancel").Triggered(); },
		[this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Up").Triggered(); },
		[this]() { auto* ic = GetInputCommand(); return ic && ic->Evaluate("Down").Triggered(); }
	);
	menuActionManager_.AddMenuAction([this]() {
		gameStartSystem_.Initialize();
		gameStartSystem_.StartSequence(1.5f);
		Application::MatsumotoUtility::SetColorToSprite(gameStartSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		Application::MatsumotoUtility::FitSpriteToTexture(gameStartSprite_);
		Application::MatsumotoUtility::SetColorToSprite(gameStartGoSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, Vector3(0.0f, 0.0f, 1.0f));
		menuActionManager_.SetMenuOpen(false);
	});
	menuActionManager_.AddMenuAction([this]() { if (auto* out = GetSceneComponent<SceneChangeOut>()) { SetNextSceneName("TitleScene"); out->Play(); } });
	menuActionManager_.AddMenuAction([this]() { if (auto* out = GetSceneComponent<SceneChangeOut>()) { SetNextSceneName("GameScene"); out->Play(); } });
	menuPosition_ = Vector2(400.0f, 300.0f);
	menuSpriteContainer_.SetPosition(menuPosition_);
	std::vector<Sprite*> menuSprites;
	for (int i = 0; i < 3; ++i) {
		auto obj = std::make_unique<Sprite>();
		obj->SetUniqueBatchKey();
		if (auto* mat = obj->GetComponent2D<Material2D>()) {
			mat->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
			mat->SetTexture(TextureManager::GetTextureFromFileName("menu_off_" + std::to_string(i) + ".png"));
		}
		Application::MatsumotoUtility::FitSpriteToTexture(obj.get());
		menuSprites.push_back(obj.get());
		obj->AttachToRenderer(sceneDefaultVariables_->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
		AddObject2D(std::move(obj));
	}
	menuSpriteContainer_.SetMenuSprite(menuSprites);
	{
		auto obj = std::make_unique<Sprite>();
		obj->SetUniqueBatchKey();
		obj->SetPivotPoint(0.5f, 0.5f);
		if (auto* mat = obj->GetComponent2D<Material2D>()) mat->SetTexture(TextureManager::GetTextureFromFileName("GameStart.png"));
		obj->AttachToRenderer(sceneDefaultVariables_->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
		gameStartSprite_ = obj.get();
		AddObject2D(std::move(obj));
		Application::MatsumotoUtility::FitSpriteToTexture(gameStartSprite_);
		Application::MatsumotoUtility::SetTranslateToSprite(gameStartSprite_, Vector2(cx, cy));
	}
	{
		auto obj = std::make_unique<Sprite>();
		obj->SetUniqueBatchKey();
		obj->SetPivotPoint(0.5f, 0.5f);
		if (auto* mat = obj->GetComponent2D<Material2D>()) mat->SetTexture(TextureManager::GetTextureFromFileName("GameStartGO.png"));
		obj->AttachToRenderer(sceneDefaultVariables_->GetMainWindow(), "Object2D.DoubleSidedCulling.BlendNormal");
		gameStartGoSprite_ = obj.get();
		AddObject2D(std::move(obj));
		Application::MatsumotoUtility::SetTranslateToSprite(gameStartGoSprite_, Vector2(cx, cy));
	}
	{
		AudioManager::PlayParams params;
		params.sound = AudioManager::GetSoundHandleFromFileName("bgmGame.mp3");
		params.volume = 0.2f;
		params.loop = true;
		audioPlayer_.AddAudio(params);
		audioPlayer_.ChangeAudio(2.0);
	}
}

void GameScene::ProcessAttack(Application::PuzzlePlayer& attacker, Application::PuzzlePlayer& defender) {
	if (!attacker.HasPendingAttack()) return;
	float garbageAmount = attacker.GetPendingGarbageToSendFloat();
	if (garbageAmount > 0.0f) {
		float surplus = attacker.OffsetGarbageQueue(garbageAmount);
		if (surplus >= 1.0f) {
			defender.EnqueueGarbage(surplus, surplus * config_.garbageDelayTimeMultiplier);
		}
	}
	attacker.ClearPendingAttack();
	attacker.ClearPendingGarbage();
}

void GameScene::CheckWinCondition() {
	if (gameOver_) return;
	if (player1_.IsDefeated()) {
		gameOver_ = true; winner_ = 2; if (resultText_) resultText_->SetText("Player 2 Wins!"); AddSceneVariable("PuzzleWinner", winner_);
	}
	else if (player2_.IsDefeated()) {
		gameOver_ = true; winner_ = 1; if (resultText_) resultText_->SetText("Player 1 Wins!"); AddSceneVariable("PuzzleWinner", winner_);
	}
	autoSceneChangeTimer_ = 3.0f;
}

void GameScene::OnUpdate() {
	if (sceneDefaultVariables_) {
		if (auto* screenBuffer2D = sceneDefaultVariables_->GetScreenBuffer2D()) {
			screenCenter_ = Vector2(
				static_cast<float>(screenBuffer2D->GetWidth()) * 0.5f,
				static_cast<float>(screenBuffer2D->GetHeight()) * 0.5f);
		}
	}
	float deltaTime = GetDeltaTime();
	menuSpriteContainer_.SetPosition(menuPosition_);
	if (backgroundSprite_) {
		Application::MatsumotoUtility::MoveTextureUVToSprite(backgroundSprite_, Vector2(0.0f, deltaTime));
	}
	if (gameStartSystem_.IsGameStarted()) menuActionManager_.Update();
	menuSpriteContainer_.SetSelectedIndex(menuActionManager_.GetSelectedIndex());
	menuSpriteContainer_.SetMenuOpen(menuActionManager_.IsMenuOpen());
	menuSpriteContainer_.Update(KashipanEngine::GetDeltaTime());
	if (auto* ic = GetInputCommand()) {
		if (ic->Evaluate("DebugSceneChange").Triggered()) {
			if (GetNextSceneName().empty()) SetNextSceneName("MenuScene");
			if (auto* out = GetSceneComponent<SceneChangeOut>()) out->Play();
		}
	}
	if (!GetNextSceneName().empty()) {
		if (auto* out = GetSceneComponent<SceneChangeOut>()) if (out->IsFinished()) ChangeToNextScene();
	}
	gameStartSystem_.Update(deltaTime);
	if (!gameOver_ && !menuActionManager_.IsMenuOpen() && gameStartSystem_.IsGameStarted()) {
		Vector3 startScale = Application::MatsumotoUtility::GetScaleFromSprite(gameStartSprite_);
		startScale.x = Application::MatsumotoUtility::SimpleEaseIn(startScale.x, 0.0f, 0.3f);
		startScale.y = Application::MatsumotoUtility::SimpleEaseIn(startScale.y, 0.0f, 0.3f);
		Application::MatsumotoUtility::SetScaleToSprite(gameStartSprite_, startScale);
		Vector3 goTextureScale = Application::MatsumotoUtility::GetTextureSizeFromSprite(gameStartGoSprite_);
		Vector4 goColor = Application::MatsumotoUtility::GetColorFromSprite(gameStartGoSprite_);
		Vector3 goScale = Application::MatsumotoUtility::GetScaleFromSprite(gameStartGoSprite_);
		goColor.w = Application::MatsumotoUtility::SimpleEaseIn(goColor.w, 0.0f, 0.1f);
		goScale.x = Application::MatsumotoUtility::SimpleEaseIn(goScale.x, goTextureScale.x, 0.3f);
		goScale.y = Application::MatsumotoUtility::SimpleEaseIn(goScale.y, goTextureScale.y, 0.3f);
		Application::MatsumotoUtility::SetColorToSprite(gameStartGoSprite_, goColor);
		Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, goScale);
		player1_.Update(deltaTime, GetInputCommand());
		if (isNPCMode_) { npc_.Update(deltaTime); player2_.Update(deltaTime, nullptr); }
		else { player2_.Update(deltaTime, GetInputCommand()); }
		ProcessAttack(player1_, player2_);
		ProcessAttack(player2_, player1_);
		CheckWinCondition();
	}
	if (!gameStartSystem_.IsGameStarted()) {
		Application::MatsumotoUtility::RotateSprite(gameStartSprite_, Vector3(0.0f, 3.14f * deltaTime, 0.0f));
	}
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
	currentColor.w = Application::MatsumotoUtility::SimpleEaseIn(currentColor.w, 0.0f, 0.05f);
	currentScale.x = Application::MatsumotoUtility::SimpleEaseIn(currentScale.x, goSpriteScale.x, 0.2f);
	currentScale.y = Application::MatsumotoUtility::SimpleEaseIn(currentScale.y, goSpriteScale.y, 0.2f);
	Application::MatsumotoUtility::SetColorToSprite(gameStartGoSprite_, currentColor);
	Application::MatsumotoUtility::SetScaleToSprite(gameStartGoSprite_, currentScale);
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
	}
}

} // namespace KashipanEngine
