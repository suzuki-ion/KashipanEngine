#include "GameTutorialManager.h"
using namespace Application;
#include <MatsumotoUtility.h>
using namespace MatsumotoUtility;

void Application::GameTutorialManager::Initialize(std::vector<KashipanEngine::Sprite*> tutorialSprite) {
	tutorialSprites_ = std::move(tutorialSprite);
	tutorialPosition_ = Vector2(0.0f, 0.0f); // チュートリアルの初期位置
	isTutorialActive_ = true;
	isPlayer_ = true;
	isGrip_ = false;

	// スプライトの数だけオフセットを設定
	tutorialSpriteOffsets_.clear();
	for (size_t i = 0; i < tutorialSprites_.size(); ++i) {
		tutorialSpriteOffsets_.emplace_back(Vector2(0.0f, 64.0f * static_cast<float>(i)));
		if (i > 2) {
			tutorialSpriteOffsets_.back() = tutorialSpriteOffsets_[2];
		}
	}
}

void Application::GameTutorialManager::Update() {
	SpriteVisible(isTutorialActive_);

	if (!isTutorialActive_) return;

	// チュートリアルの更新処理（例: スプライトの位置を更新）
	for(int i = 0; i < tutorialSprites_.size(); ++i) {
		if (i < tutorialSpriteOffsets_.size()) {
			tutorialSprites_[i]->SetAnchorPoint(0.5f, 0.5f); // アンカーポイントを中央に設定
			if (auto* tr = tutorialSprites_[i]->GetComponent2D<KashipanEngine::Transform2D>()) {
				tr->SetTranslate(Vector3(tutorialPosition_.x + tutorialSpriteOffsets_[i].x,
					tutorialPosition_.y + tutorialSpriteOffsets_[i].y, 0.0f));
			}
		}
	}

	// グリップしてたら3番を表示、４番を非表示
	if (isGrip_) {
		if (auto* mt = tutorialSprites_[2]->GetComponent2D<KashipanEngine::Material2D>()) {
			mt->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.0f));
		}
		if (auto* mt = tutorialSprites_[3]->GetComponent2D<KashipanEngine::Material2D>()) {
			mt->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}
	} else {
		if (auto* mt = tutorialSprites_[2]->GetComponent2D<KashipanEngine::Material2D>()) {
			mt->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		if (auto* mt = tutorialSprites_[3]->GetComponent2D<KashipanEngine::Material2D>()) {
			mt->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.0f));
		}
	}

	// 5番は1番の横に表示
	if (auto* tr = tutorialSprites_[4]->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetTranslate(Vector3(tutorialPosition_.x + 256.0f, tutorialPosition_.y, 0.0f));
	}
	
	// すべてのスプライトの色を更新
	for(auto* sprite : tutorialSprites_) {
		Vector4 color = GetColorFromSprite(sprite);
		color.x = SimpleEaseIn(color.x, 1.0f, 0.1f);
		color.y = SimpleEaseIn(color.y, 1.0f, 0.1f);
		color.z = SimpleEaseIn(color.z, 1.0f, 0.1f);
		SetColorToSprite(sprite, color);
	}

	// グリップしているなら１番を黒くする
	ChangeSpriteColorRGB(tutorialSprites_[0], isGrip_ ? Vector3(0.3f, 0.3f, 0.3f) : Vector3(1.0f, 1.0f, 1.0f));

	// 送っているなら２番を黒くする
	ChangeSpriteColorRGB(tutorialSprites_[1], isSend_ ? Vector3(0.3f, 0.3f, 0.3f) : Vector3(1.0f, 1.0f, 1.0f));

	// 動いているなら３番,4番を黒くする
	ChangeSpriteColorRGB(tutorialSprites_[2], isMove_ ? Vector3(0.3f, 0.3f, 0.3f) : Vector3(1.0f, 1.0f, 1.0f));
	ChangeSpriteColorRGB(tutorialSprites_[3], isMove_ ? Vector3(0.3f, 0.3f, 0.3f) : Vector3(1.0f, 1.0f, 1.0f));

	// 入れ替わっているなら５番を黒くする
	ChangeSpriteColorRGB(tutorialSprites_[4], isSwap_ ? Vector3(0.3f, 0.3f, 0.3f) : Vector3(1.0f, 1.0f, 1.0f));
}

void Application::GameTutorialManager::SpriteVisible(bool visible) {
	for (auto* sprite : tutorialSprites_) {
		if (auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
			Vector4 color = mat->GetColor();
			color.w = visible ? 1.0f : 0.0f; 
			mat->SetColor(color);
		}
	}
}
