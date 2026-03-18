#include "TitleSpriteManager.h"
#include <MatsumotoUtility.h>
using namespace Application::MatsumotoUtility;

void Application::TitleSpriteManager::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc)
{
	CreateSpriteFunc_ = createSpriteFunc;
	float windowWidth = 1920.0f;
	float windowHeight = 1080.0f;
	centerPosition_ = Vector3(windowWidth * 0.5f, windowHeight *0.5f, 0.0f);
	triggered1PTimer_ = 0.0f;
	triggered2PTimer_ = 0.0f;

	previousSection_ = TitleSection::TitleCall;

	// セクションごとの更新関数を登録しておく
	sectionUpdateFunctions_[TitleSection::TitleCall] = [this]() { UpdateTitleCallSection(); };
	sectionUpdateFunctions_[TitleSection::ModeSelect] = [this]() { UpdateModeSelectSection(); };
	sectionUpdateFunctions_[TitleSection::AISelect] = [this]() { UpdateAISelectSection(); };
	sectionUpdateFunctions_[TitleSection::MultiplayerSelect] = [this]() { UpdateMultiplayerSelectSection(); };

	// ここで必要なスプライトを生成しておく
	sprites_["Root"] = CreateSpriteFunc_("Root", KashipanEngine::DefaultSampler::LinearClamp);
	SetTranslateToSprite(sprites_["Root"], centerPosition_);

	// タイトルの背景スプライト
	sprites_["TitleBackground"] = CreateSpriteFunc_("TitleBackground", KashipanEngine::DefaultSampler::LinearWrap);
	SetTextureToSprite(sprites_["TitleBackground"], "TitleBG.png");
	FitSpriteToTexture(sprites_["TitleBackground"]);
	SetTranslateToSprite(sprites_["TitleBackground"], centerPosition_);

	// タイトル画面のスプライト
	sprites_["TitleScreen"] = CreateSpriteFunc_("TitleScreen", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["TitleScreen"], sprites_["Root"]);
	SetTextureToSprite(sprites_["TitleScreen"], "TitleScreen.png");
	FitSpriteToTexture(sprites_["TitleScreen"]);
	ScaleSprite(sprites_["TitleScreen"], 3.4f);

	// タイトルロゴのスプライト
	sprites_["TitleLogo"] = CreateSpriteFunc_("TitleLogo", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["TitleLogo"], sprites_["Root"]);
	SetTextureToSprite(sprites_["TitleLogo"], "TitleLogo.png");
	FitSpriteToTexture(sprites_["TitleLogo"]);

	// 2Pの画面スプライト
	sprites_["TitleScreen2P"] = CreateSpriteFunc_("TitleScreen2P", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["TitleScreen2P"], sprites_["Root"]);
	SetTextureToSprite(sprites_["TitleScreen2P"], "TitleScreen.png");
	FitSpriteToTexture(sprites_["TitleScreen2P"]);
	ScaleSprite(sprites_["TitleScreen2P"], 3.4f);
	SetTranslateToSprite(sprites_["TitleScreen2P"], Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f));

	// 1Pの姿見
	sprites_["P1"] = CreateSpriteFunc_("Player1Preview", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["P1"], sprites_["Root"]);
	SetTextureToSprite(sprites_["P1"], "fall_Porn.png");
	FitSpriteToTexture(sprites_["P1"]);
	ScaleSprite(sprites_["P1"], 2.0f);
	SetTranslateToSprite(sprites_["P1"], Vector3(-centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f));
	// 2Pの姿見
	sprites_["P2"] = CreateSpriteFunc_("Player2Preview", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["P2"], sprites_["Root"]);
	SetTextureToSprite(sprites_["P2"], "fall_Porn.png");
	FitSpriteToTexture(sprites_["P2"]);
	ScaleSprite(sprites_["P2"], 2.0f);
	FlipSpriteHorizontal(sprites_["P2"]);
	SetTranslateToSprite(sprites_["P2"], Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f));
	
	// 2P側AIの姿見
	sprites_["P2AI"] = CreateSpriteFunc_("Player2AIPreview", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["P2AI"], sprites_["Root"]);
	SetTextureToSprite(sprites_["P2AI"], "porn_ai.png");
	FitSpriteToTexture(sprites_["P2AI"]);
	ScaleSprite(sprites_["P2AI"], 6.0f);
	SetTranslateToSprite(sprites_["P2AI"], Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f));

	// コントローラー同時押しの案内の背景
	sprites_["SimultaneousSubmitGuideBG"] = CreateSpriteFunc_("SimultaneousSubmitGuideBG", KashipanEngine::DefaultSampler::LinearClamp);
	SetTextureToSprite(sprites_["SimultaneousSubmitGuideBG"], "ControllerBG.png");
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], Vector3(centerPosition_.x,centerPosition_.y + windowHeight,centerPosition_.z));
	FitSpriteToTexture(sprites_["SimultaneousSubmitGuideBG"]);
	ScaleSprite(sprites_["SimultaneousSubmitGuideBG"], 0.6f);

	// 1Pのコントローラー
	sprites_["ControllerP1"] = CreateSpriteFunc_("ControllerP1", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["ControllerP1"], sprites_["SimultaneousSubmitGuideBG"]);
	SetTextureToSprite(sprites_["ControllerP1"], "controller.png");
	SetScaleToSprite(sprites_["ControllerP1"], Vector3(0.3f, 0.5f, 1.0f));
	SetTranslateToSprite(sprites_["ControllerP1"], Vector3(-0.2f, 0.0f, 0.0f));
	// 2Pのコントローラー
	sprites_["ControllerP2"] = CreateSpriteFunc_("ControllerP2", KashipanEngine::DefaultSampler::LinearClamp);
	ParentSpriteToSprite(sprites_["ControllerP2"], sprites_["SimultaneousSubmitGuideBG"]);
	SetTextureToSprite(sprites_["ControllerP2"], "controller.png");
	SetScaleToSprite(sprites_["ControllerP2"], Vector3(0.3f, 0.5f, 1.0f));
	SetTranslateToSprite(sprites_["ControllerP2"], Vector3(0.2f, 0.0f, 0.0f));

	// タイトルのバー
	sprites_["TitleBarDown"] = CreateSpriteFunc_("titleBar_Down", KashipanEngine::DefaultSampler::LinearWrap);
	SetTextureToSprite(sprites_["TitleBarDown"], "titleBar_Down.png");
	SetTranslateToSprite(sprites_["TitleBarDown"], Vector3(centerPosition_.x, centerPosition_.y, 0.0f));
	FitSpriteToTexture(sprites_["TitleBarDown"]);

	sprites_["TitleBarUp"] = CreateSpriteFunc_("titleBar_Up", KashipanEngine::DefaultSampler::LinearWrap);
	SetTextureToSprite(sprites_["TitleBarUp"], "titleBar_Up.png");
	SetTranslateToSprite(sprites_["TitleBarUp"], Vector3(centerPosition_.x, centerPosition_.y, 0.0f));
	FitSpriteToTexture(sprites_["TitleBarUp"]);

	// タイトルのセクション表示
	sprites_["TitleCallSection"] = CreateSpriteFunc_("TitleCallSection", KashipanEngine::DefaultSampler::LinearClamp);
	SetTextureToSprite(sprites_["TitleCallSection"], "title_Bar_Mode.png");
	FitSpriteToTexture(sprites_["TitleCallSection"]);
	SetTranslateToSprite(sprites_["TitleCallSection"], Vector3(-centerPosition_.x, centerPosition_.y, 0.0f));
}

void Application::TitleSpriteManager::Update(float deltaTime, TitleSection currentSection, int selectNumber)
{
	deltaTime_ = deltaTime;
	currentSection_ = currentSection;
	currentSelectNumber_ = selectNumber;

	timer_ += deltaTime;

	// セクションが変わった
	if(previousSection_ != currentSection_) {
		SetTranslateToSprite(sprites_["TitleCallSection"], Vector3(-centerPosition_.x, centerPosition_.y, 0.0f));
		if(currentSection_ == TitleSection::ModeSelect) {
			SetTextureToSprite(sprites_["TitleCallSection"], "title_Bar_Mode.png");
		}
		else if(currentSection_ == TitleSection::AISelect) {
			SetTextureToSprite(sprites_["TitleCallSection"], "title_Bar_Ai.png");
		}
		else if(currentSection_ == TitleSection::MultiplayerSelect) {
			SetTextureToSprite(sprites_["TitleCallSection"], "title_Bar_Friend.png");
		}
	}

	// 1pと2pの姿見は揺れ続ける
	SetRotationToSprite(sprites_["P1"], Vector3(0.0f, 0.0f, sinf(timer_) * 0.1f));
	SetRotationToSprite(sprites_["P2"], Vector3(0.0f, 0.0f, cosf(timer_) * 0.1f));
	SetRotationToSprite(sprites_["P2AI"], Vector3(0.0f, 0.0f, cosf(timer_ + 1.0f) * 0.1f)); // 2PのAIの姿見は少し位相をずらして揺れるようにする
	
	// タイトル背景のUVスクロール
	float scrollSpeed = 2.0f; // スクロール速度
	if (currentSection_ == TitleSection::AISelect || currentSection_ == TitleSection::MultiplayerSelect) {
		scrollSpeed = 5.0f; // AI選択とマルチプレイヤー選択のセクションではスクロール速度を速くする
	}
	MoveTextureUVToSprite(sprites_["TitleBackground"], Vector2(0.0f, deltaTime * scrollSpeed));

	// 手振れを加えるための更新関数も呼び出す
	if (currentSection_ != TitleSection::TitleCall) { // タイトルコールのセクションでは手振れを加えない
		UpdateIdleSection();
		// タイトル以外だったらタイトルのバーを出す
		SimpleEaseSpriteMove(sprites_["TitleBarDown"], Vector3(centerPosition_.x, centerPosition_.y, 0.0f), 0.3f);
		SimpleEaseSpriteMove(sprites_["TitleBarUp"], Vector3(centerPosition_.x, centerPosition_.y, 0.0f), 0.3f);
		SimpleEaseSpriteMove(sprites_["TitleCallSection"], Vector3(centerPosition_.x, centerPosition_.y, 0.0f), 0.3f);
	}
	else {
		// タイトルロゴを揺らす
		SetRotationToSprite(sprites_["TitleLogo"], Vector3(0.0f, 0.0f, sinf(timer_) * 0.05f));

		SimpleEaseSpriteMove(sprites_["TitleBarDown"], Vector3(centerPosition_.x, centerPosition_.y - centerPosition_.y, 0.0f), 0.3f);
		SimpleEaseSpriteMove(sprites_["TitleBarUp"], Vector3(centerPosition_.x, centerPosition_.y + centerPosition_.y, 0.0f), 0.3f);
	}

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

	previousSection_ = currentSection_;
}

void Application::TitleSpriteManager::UpdateTitleCallSection()
{
	// スケールを初期位置に
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x,1.0f,0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 1.0f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	// 回転を初期位置に
	Vector3 currentRotation = GetRotationFromSprite(sprites_["Root"]);
	currentRotation.z = SimpleEaseIn(currentRotation.z, 0.0f, 0.3f);
	SetRotationToSprite(sprites_["Root"], currentRotation);

	// 位置を初期位置に
	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x, 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	// 1Pの姿見を初期位置に
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
	currentScales.x = SimpleEaseIn(currentScales.x, 0.3f, 0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.3f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x - (centerPosition_.x*0.5f), 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	// 回転を初期位置に
	Vector3 currentRotation = GetRotationFromSprite(sprites_["Root"]);
	currentRotation.z = SimpleEaseIn(currentRotation.z, 0.1f, 0.3f);
	SetRotationToSprite(sprites_["Root"], currentRotation);

	Vector3 targetTranslateP1(-centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f);
	Vector3 currentTranslateP1 = GetTranslateFromSprite(sprites_["P1"]);
	currentTranslateP1.x = SimpleEaseIn(currentTranslateP1.x, targetTranslateP1.x, 0.3f);
	currentTranslateP1.y = SimpleEaseIn(currentTranslateP1.y, targetTranslateP1.y, 0.3f);
	SetTranslateToSprite(sprites_["P1"], currentTranslateP1);

	// 2Pの位置は選択によって変える
	Vector3 targetTranslateP2 = Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f);
	Vector3 targetTranslateP2AI = Vector3(centerPosition_.x * 3.0f, 0.0f, 0.0f) + Vector3(centerPosition_.x * 0.9f, -centerPosition_.y * 0.9f, 0.0f);
	if(currentSelectNumber_ == 0) {
		targetTranslateP2.x = centerPosition_.x * 10.0f;
	} else {
		targetTranslateP2AI.x = centerPosition_.x * 10.0f;
	}
	// 2Pの位置を変える
	Vector3 currentTranslateP2 = GetTranslateFromSprite(sprites_["P2"]);
	currentTranslateP2.x = SimpleEaseIn(currentTranslateP2.x, targetTranslateP2.x, 0.3f);
	currentTranslateP2.y = SimpleEaseIn(currentTranslateP2.y, targetTranslateP2.y, 0.3f);
	SetTranslateToSprite(sprites_["P2"], currentTranslateP2);
	// 2Paiの位置を変える
	Vector3 currentTranslateP2AI = GetTranslateFromSprite(sprites_["P2AI"]);
	currentTranslateP2AI.x = SimpleEaseIn(currentTranslateP2AI.x, targetTranslateP2AI.x, 0.3f);
	currentTranslateP2AI.y = SimpleEaseIn(currentTranslateP2AI.y, targetTranslateP2AI.y, 0.3f);
	SetTranslateToSprite(sprites_["P2AI"], currentTranslateP2AI);

	SetTextureToSprite(sprites_["TitleScreen2P"], "TitleScreen.png");

	// コントローラー同時押しの案内の背景を表示
	Vector3 currentTranslateMultiBG = GetTranslateFromSprite(sprites_["SimultaneousSubmitGuideBG"]);
	currentTranslateMultiBG.y = SimpleEaseIn(currentTranslateMultiBG.y, centerPosition_.y + centerPosition_.y * 2.0f, 0.3f);
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], currentTranslateMultiBG);
}

void Application::TitleSpriteManager::UpdateAISelectSection()
{
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x, 0.6f, 0.3f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.6f, 0.3f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, -centerPosition_.x * 0.8f, 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y * 0.9f, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	std::string textureName = "ai_" + std::to_string(currentSelectNumber_) + ".png";
	SetTextureToSprite(sprites_["TitleScreen2P"], textureName);

}

void Application::TitleSpriteManager::UpdateMultiplayerSelectSection()
{
	Vector3 currentScales = GetScaleFromSprite(sprites_["Root"]);
	currentScales.x = SimpleEaseIn(currentScales.x, 0.2f, 0.2f);
	currentScales.y = SimpleEaseIn(currentScales.y, 0.2f, 0.2f);
	SetScaleToSprite(sprites_["Root"], currentScales);

	Vector3 currentTranslate = GetTranslateFromSprite(sprites_["Root"]);
	currentTranslate.x = SimpleEaseIn(currentTranslate.x, centerPosition_.x - (centerPosition_.x * 0.3f), 0.3f);
	currentTranslate.y = SimpleEaseIn(currentTranslate.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["Root"], currentTranslate);

	Vector3 currentTranslateMultiBG = GetTranslateFromSprite(sprites_["SimultaneousSubmitGuideBG"]);
	currentTranslateMultiBG.y = SimpleEaseIn(currentTranslateMultiBG.y, centerPosition_.y, 0.3f);
	SetTranslateToSprite(sprites_["SimultaneousSubmitGuideBG"], currentTranslateMultiBG);

	Vector3 currentControllerP1Translate = GetTranslateFromSprite(sprites_["ControllerP1"]);
	currentControllerP1Translate.y = SimpleEaseIn(currentControllerP1Translate.y, triggered1PTimer_ * triggered1PTimer_ * 0.1f, 0.3f);
	SetTranslateToSprite(sprites_["ControllerP1"], currentControllerP1Translate);

	Vector3 currentControllerP2Translate = GetTranslateFromSprite(sprites_["ControllerP2"]);
	currentControllerP2Translate.y = SimpleEaseIn(currentControllerP2Translate.y, triggered2PTimer_ * triggered2PTimer_ * 0.1f, 0.3f);
	SetTranslateToSprite(sprites_["ControllerP2"], currentControllerP2Translate);
}

void Application::TitleSpriteManager::UpdateIdleSection() {
	// 手振れのような微妙な回転を加える
	float shakeAmount = 0.03f; // 回転の振れ幅
	Vector3 rotation{0.0f,0.0f,0.0f};
	rotation.x += shakeAmount * sinf(timer_ * 0.5f);
	rotation.y += shakeAmount * sinf(timer_ * 0.6f);
	rotation.z += shakeAmount * sinf(timer_ * 0.4f);
	SetRotationToSprite(sprites_["Root"], rotation);
}
