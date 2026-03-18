#include "MenuSpriteCotainer.h"
using namespace Application;
using namespace KashipanEngine;
#include "MatsumotoUtility.h"

void MenuSpriteContainer::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSpriteFunc) {

	// スプライトの生成関数を保存
	createSpriteFunc_ = createSpriteFunc;

	timer_ = 0.0f;

	menuSprites_.clear();
	selectedIndex_ = 0;
	float w = 1920.0f; // 仮の画面幅
	float h = 1080.0f; // 仮の画面高さ
	screenCenter_ = Vector2(w * 0.5f, h * 0.5f);

	menuAnchorPoint_ = screenCenter_ + Vector2(screenCenter_.x*0.25f,0.0f);
	rotateOffset_ = 0.0f;

	// メニューの背景スプライトを生成
	menuBGSprite_ = createSpriteFunc_("MenuBGSprite", "white1x1.png");
	MatsumotoUtility::SetScaleToSprite(menuBGSprite_, Vector3(w, h, 1.0f));
	MatsumotoUtility::SetColorToSprite(menuBGSprite_, Vector4(0.3f, 0.3f, 0.3f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(menuBGSprite_, Vector3(screenCenter_.x, screenCenter_.y, 0.0f));

	// コントローラーの入力を可視化するオブジェクトの初期化
	controllerViewer_.Initialize(createSpriteFunc_);
	controllerViewer_.SetTranslate(Vector3(screenCenter_.x * 1.5f, screenCenter_.y*0.9f, 0.0f));
	controllerViewer_.SetRotation(Vector3(0.0f, 0.0f, -0.5f));

	// メニューのスプライトは3つと仮定
	menuSpriteCount_ = 3;
	for (int i = 0; i < menuSpriteCount_; ++i) {
		menuSprites_.push_back(
			createSpriteFunc_("MenuSprite" + std::to_string(i), "menu_off_" + std::to_string(i) + ".png"));
	}

	// メニューの文字を生成
	menuTextSprites_.clear();
	int menuTextCount = 3; // 仮のメニューの数
	for (int i = 0; i < menuTextCount; i++) {
		menuTextSprites_.push_back(
			createSpriteFunc_("MenuTextSprite" + std::to_string(i), "pause_" + std::to_string(i) + ".png"));
	}
}

void MenuSpriteContainer::Update(float delta) {
	timer_ += delta;

	// メニューが閉じているならすべて
	if (!isMenuOpen_) {
		Vector3 targetPositionUp = menuAnchorPoint_ + Vector2(0.0f, screenCenter_.y * 2.0f);
		
		// メニューが閉じているならメニューのスプライトを左に移動させる
		for (size_t i = 0; i < menuSprites_.size(); ++i) {
			if (menuSprites_[i]) {
				Vector3 targetPositionLeft = menuAnchorPoint_ - Vector2(screenCenter_.x, (static_cast<float>(i)) * 150.0f);
				MatsumotoUtility::SimpleEaseSpriteMove(menuSprites_[i], targetPositionLeft, 0.3f);
				MatsumotoUtility::SimpleEaseSpriteColor(menuBGSprite_, Vector4(0.3f, 0.3f, 0.3f, 0.0f), 0.3f);
			}
		}
		// メニューが閉じているならメニューの文字も上に移動させる
		for(int i = 0; i < menuTextSprites_.size(); ++i) {
			if(menuTextSprites_[i]) {
				MatsumotoUtility::SimpleEaseSpriteMove(menuTextSprites_[i], targetPositionUp, 0.3f);
			}
		}
		// メニューが閉じているならコントローラーの入力を可視化するオブジェクトも右に移動させる
		Vector3 currentTranslate = controllerViewer_.GetTranslate();
		currentTranslate.x = MatsumotoUtility::SimpleEaseIn(currentTranslate.x, screenCenter_.x * 3.0f, 0.3f);
		controllerViewer_.SetTranslate(currentTranslate);
		return;
	}

	// メニューが開いているなら背景の色を徐々に濃くする
	MatsumotoUtility::SimpleEaseSpriteColor(menuBGSprite_, Vector4(0.3f, 0.3f, 0.3f, 0.5f), 0.3f);

	// メニューが開いているならコントローラーの入力を可視化するオブジェクトを徐々に表示する
	Vector3 currentTranslate = controllerViewer_.GetTranslate();
	currentTranslate.x = MatsumotoUtility::SimpleEaseIn(currentTranslate.x, screenCenter_.x * 1.5f, 0.3f);
	currentTranslate.y = MatsumotoUtility::SimpleEaseIn(currentTranslate.y, screenCenter_.y * 0.9f + sinf(timer_) * 5.0f, 0.3f);
	controllerViewer_.SetTranslate(currentTranslate);

	// アンカーポイントを基準にスプライトの位置を更新,選択されているならテクスチャを変える
	for (size_t i = 0; i < menuSprites_.size(); ++i) {
		if (menuSprites_[i]) {
			// スプライトの位置の更新
			Vector3 offset(0.0f, (static_cast<float>(i)) * 150.0f, 0.0f); // 選択されているスプライトがアンカーポイントに来るようにオフセットを計算
			if (selectedIndex_ == static_cast<int>(i)) {
				offset.x += screenCenter_.x * 0.1f; // 選択されているスプライトは右に少し移動させる
			}			
			offset += menuAnchorPoint_; // アンカーポイントを加算
			// 現在のスプライトの位置を取得して、オフセットに向かってイージングで移動させる
			MatsumotoUtility::SimpleEaseSpriteMove(menuSprites_[i], offset, 0.1f);

			// 選択されているスプライトのテクスチャを変える
			if (selectedIndex_ == static_cast<int>(i)) {
				MatsumotoUtility::SetTextureToSprite(menuSprites_[i], "menu_on_"+ std::to_string(i) + ".png");
			}
			else {
				MatsumotoUtility::SetTextureToSprite(menuSprites_[i], "menu_off_" + std::to_string(i) + ".png");
			}
		}
	}

	// メニューの文字の位置を更新
	for (size_t i = 0; i < menuTextSprites_.size(); ++i) {
		if (menuTextSprites_[i]) {
			Vector3 offset(
				(static_cast<float>(i)) * 150.0f - screenCenter_.x * 0.2f, 
				screenCenter_.y + sinf(timer_*2.0f + static_cast<float>(i)) * screenCenter_.y * 0.05f, 0.0f); // 選択されているスプライトがアンカーポイントに来るようにオフセットを計算
			offset += menuAnchorPoint_; // アンカーポイントを加算
			MatsumotoUtility::SimpleEaseSpriteMove(menuTextSprites_[i], offset, 0.1f);
		}
	}
}

void MenuSpriteContainer::SetPosition(const Vector2& position) {
	menuAnchorPoint_ = position;
}

void Application::MenuSpriteContainer::SetSelectedIndex(int index)
{
	if (index < 0 || index >= static_cast<int>(menuSprites_.size())) {
		return; // 範囲外のインデックスは無視
	}
	selectedIndex_ = index;
}

void Application::MenuSpriteContainer::SetMenuSprite(std::vector<KashipanEngine::Sprite*> sprite)
{
	menuSprites_ = std::move(sprite);
}
