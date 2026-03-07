#include "TitleSpriteManager.h"
#include <MatsumotoUtility.h>
using namespace Application::MatsumotoUtility;

void Application::TitleSpriteManager::Initialize(std::function<KashipanEngine::Sprite* (const std::string&)> createSpriteFunc)
{
	CreateSpriteFunc_ = createSpriteFunc;
	float windowWidth = 1920.0f;
	float windowHeight = 1080.0f;
	centerPosition_ = Vector3(windowWidth * 0.5f, windowHeight *0.5f, 0.0f);

	// セクションごとの更新関数を登録しておく
	sectionUpdateFunctions_[TitleSection::TitleCall] = [this]() { UpdateTitleCallSection(); };
	sectionUpdateFunctions_[TitleSection::ModeSelect] = [this]() { UpdateModeSelectSection(); };
	sectionUpdateFunctions_[TitleSection::AISelect] = [this]() { UpdateAISelectSection(); };
	sectionUpdateFunctions_[TitleSection::MultiplayerSelect] = [this]() { UpdateMultiplayerSelectSection(); };

	// ここで必要なスプライトを生成しておく
	sprites_["Root"] = CreateSpriteFunc_("Root");
	SetTranslateToSprite(sprites_["Root"], centerPosition_);

	// タイトル画面のスプライト
	sprites_["TitleScreen"] = CreateSpriteFunc_("TitleScreen");
	ParentSpriteToSprite(sprites_["TitleScreen"], sprites_["Root"]);
	SetTextureToSprite(sprites_["TitleScreen"], "TitleScreen.png");
	FitSpriteToTexture(sprites_["TitleScreen"]);
	ScaleSprite(sprites_["TitleScreen"], 3.4f);

	// 2Pの画面スプライト
	sprites_["TitleScreen2P"] = CreateSpriteFunc_("TitleScreen2P");
	ParentSpriteToSprite(sprites_["TitleScreen2P"], sprites_["Root"]);
	SetTextureToSprite(sprites_["TitleScreen2P"], "TitleScreen.png");
	FitSpriteToTexture(sprites_["TitleScreen2P"]);
	ScaleSprite(sprites_["TitleScreen2P"], 3.4f);
	SetTranslateToSprite(sprites_["TitleScreen2P"], Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f));

	// 1Pの姿見
	sprites_["P1"] = CreateSpriteFunc_("Player1Preview");
	ParentSpriteToSprite(sprites_["P1"], sprites_["Root"]);
	SetTextureToSprite(sprites_["P1"], "porn.png");
	FitSpriteToTexture(sprites_["P1"]);
	ScaleSprite(sprites_["P1"], 6.0f);
	SetTranslateToSprite(sprites_["P1"], Vector3(-centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f));
	// 2Pの姿見
	sprites_["P2"] = CreateSpriteFunc_("Player2Preview");
	ParentSpriteToSprite(sprites_["P2"], sprites_["Root"]);
	SetTextureToSprite(sprites_["P2"], "porn.png");
	FitSpriteToTexture(sprites_["P2"]);
	ScaleSprite(sprites_["P2"], 6.0f);
	SetTranslateToSprite(sprites_["P2"], Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f));

	// コントローラー同時押しの案内の背景
	sprites_["SimultaneousSubmitGuideBG"] = CreateSpriteFunc_("SimultaneousSubmitGuideBG");
	SetScaleToSprite(sprites_["SimultaneousSubmitGuideBG"], Vector3(windowWidth*0.8f, windowHeight * 0.8f, 1.0f));
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], Vector3(centerPosition_.x,centerPosition_.y + windowHeight,centerPosition_.z));
	SetColorToSprite(sprites_["SimultaneousSubmitGuideBG"], Vector4(0.0f, 0.0f, 0.0f, 0.8f));

	// 1Pのコントローラー
	sprites_["ControllerP1"] = CreateSpriteFunc_("ControllerP1");
	ParentSpriteToSprite(sprites_["ControllerP1"], sprites_["SimultaneousSubmitGuideBG"]);
	SetTextureToSprite(sprites_["ControllerP1"], "controller.png");
	SetScaleToSprite(sprites_["ControllerP1"], Vector3(0.3f, 0.5f, 1.0f));
	SetTranslateToSprite(sprites_["ControllerP1"], Vector3(-0.2f, 0.0f, 0.0f));
	// 2Pのコントローラー
	sprites_["ControllerP2"] = CreateSpriteFunc_("ControllerP2");
	ParentSpriteToSprite(sprites_["ControllerP2"], sprites_["SimultaneousSubmitGuideBG"]);
	SetTextureToSprite(sprites_["ControllerP2"], "controller.png");
	SetScaleToSprite(sprites_["ControllerP2"], Vector3(0.3f, 0.5f, 1.0f));
	SetTranslateToSprite(sprites_["ControllerP2"], Vector3(0.2f, 0.0f, 0.0f));
}

void Application::TitleSpriteManager::Update(float deltaTime, TitleSection currentSection, int selectNumber)
{
	deltaTime_ = deltaTime;
	currentSection_ = currentSection;
	currentSelectNumber_ = selectNumber;
	
	// 現在のセクションに対応する処理を呼び出す
	auto it = sectionUpdateFunctions_.find(currentSection_);
	if (it != sectionUpdateFunctions_.end()) {
		it->second();
	}

	// デバッグ
	ImGui::Begin("TitleSpriteManager Debug");
	Vector3 titleScreenTranslate = GetTranslateFromSprite(sprites_["Root"]);
	Vector3 titleScreenScale = GetScaleFromSprite(sprites_["Root"]);
	Vector3 titleScreenRotation = GetRotationFromSprite(sprites_["Root"]);

	ImGui::Text("Delta Time: %.4f", deltaTime_);
	ImGui::Text("Current Section: %d", static_cast<int>(currentSection_));
	ImGui::Text("Current Select Number: %d", currentSelectNumber_);

	ImGui::Spacing();

	ImGui::Text("TitleScreen Root Transform");
	ImGui::DragFloat3("Root Translate", &titleScreenTranslate.x, 1.0f);
	ImGui::DragFloat3("Root Scale", &titleScreenScale.x, 0.01f);
	ImGui::DragFloat3("Root Rotation", &titleScreenRotation.x, 0.01f);

	SetTranslateToSprite(sprites_["Root"], titleScreenTranslate);
	SetScaleToSprite(sprites_["Root"], titleScreenScale);
	SetRotationToSprite(sprites_["Root"], titleScreenRotation);

	ImGui::End();
}

void Application::TitleSpriteManager::UpdateTitleCallSection()
{
	// スケールを初期位置に
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x,1.0f,0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 1.0f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	// 位置を初期位置に
	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x, 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	Vector3 targetTranslateP1(-centerPosition_.x * 2.0f, -centerPosition_.y * 2.0f, 0.0f);
	Vector3 currentTranslateP1 = GetTranslateFromSprite(sprites_["P1"]);
	currentTranslateP1.x = SimpleEaseIn(currentTranslateP1.x, targetTranslateP1.x, 0.3f);
	currentTranslateP1.y = SimpleEaseIn(currentTranslateP1.y, targetTranslateP1.y, 0.3f);
	SetTranslateToSprite(sprites_["P1"], currentTranslateP1);

	// コントローラー同時押しの案内の背景を表示
	Vector3 currentTranslateMultiBG = GetTranslateFromSprite(sprites_["SimultaneousSubmitGuideBG"]);
	currentTranslateMultiBG.y = SimpleEaseIn(currentTranslateMultiBG.y, centerPosition_.y + centerPosition_.y * 2.0f, 0.3f);
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], currentTranslateMultiBG);
}

void Application::TitleSpriteManager::UpdateModeSelectSection()
{
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x, 0.2f, 0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.2f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x - (centerPosition_.x * 0.3f), 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	Vector3 targetTranslateP1(-centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f);
	Vector3 currentTranslateP1 = GetTranslateFromSprite(sprites_["P1"]);
	currentTranslateP1.x = SimpleEaseIn(currentTranslateP1.x, targetTranslateP1.x, 0.3f);
	currentTranslateP1.y = SimpleEaseIn(currentTranslateP1.y, targetTranslateP1.y, 0.3f);
	SetTranslateToSprite(sprites_["P1"], currentTranslateP1);

	Vector3 targetTranslateP2 = Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f);
	if(currentSelectNumber_ == 0) {
		targetTranslateP2.x = centerPosition_.x * 10.0f;
	}
	Vector3 currentTranslateP2 = GetTranslateFromSprite(sprites_["P2"]);
	currentTranslateP2.x = SimpleEaseIn(currentTranslateP2.x, targetTranslateP2.x, 0.3f);
	currentTranslateP2.y = SimpleEaseIn(currentTranslateP2.y, targetTranslateP2.y, 0.3f);
	SetTranslateToSprite(sprites_["P2"], currentTranslateP2);

	SetTextureToSprite(sprites_["TitleScreen2P"], "TitleScreen.png");

	// コントローラー同時押しの案内の背景を表示
	Vector3 currentTranslateMultiBG = GetTranslateFromSprite(sprites_["SimultaneousSubmitGuideBG"]);
	currentTranslateMultiBG.y = SimpleEaseIn(currentTranslateMultiBG.y, centerPosition_.y + centerPosition_.y * 2.0f, 0.3f);
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], currentTranslateMultiBG);
}

void Application::TitleSpriteManager::UpdateAISelectSection()
{
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x, 0.8f, 0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.8f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, -centerPosition_.x * 1.4f, 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	std::string textureName = "ai_" + std::to_string(currentSelectNumber_) + ".png";
	SetTextureToSprite(sprites_["TitleScreen2P"], textureName);

}

void Application::TitleSpriteManager::UpdateMultiplayerSelectSection()
{
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x, 0.15f, 0.2f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.15f, 0.2f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x - (centerPosition_.x * 0.3f), 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	Vector3 currentTranslateMultiBG = GetTranslateFromSprite(sprites_["SimultaneousSubmitGuideBG"]);
	currentTranslateMultiBG.y = SimpleEaseIn(currentTranslateMultiBG.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], currentTranslateMultiBG);
}
