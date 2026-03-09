#include "MenuSpriteCotainer.h"
using namespace Application;
using namespace KashipanEngine;
#include "MatsumotoUtility.h"

void MenuSpriteContainer::Initialize() {
	menuSprites_.clear();
	menuSpriteOffsets_.clear();
	selectedIndex_ = 0;
	menuAnchorPoint_ = { 0.0f, 0.0f };
	rotateOffset_ = 0.0f;
}

void MenuSpriteContainer::Update(float delta) {
	delta; // 現状はdeltaは使用しないが、将来的にアニメーションなどに使用する可能性があるため引数として受け取る

	// メニューが閉じているならすべての透明度を0にして早期リターン
	if (!isMenuOpen_) {
		for (size_t i = 0; i < menuSprites_.size(); ++i) {
			if (menuSprites_[i]) {
				// スプライトのTransform2Dコンポーネントを取得してスケールを設定
				if (auto* tr = menuSprites_[i]->GetComponent2D<Material2D>()) {
					tr->SetColor(Vector4(1.0f, 1.0f, 1.0f, 0.0f)); // 透明度を0に
				}
			}
		}
		return;
	}
	else {
		// メニューが開いているならすべての透明度を1にする
		for (size_t i = 0; i < menuSprites_.size(); ++i) {
			if (menuSprites_[i]) {
				// スプライトのTransform2Dコンポーネントを取得してスケールを設定
				if (auto* tr = menuSprites_[i]->GetComponent2D<Material2D>()) {
					tr->SetColor(Vector4(1.0f, 1.0f, 1.0f, 1.0f)); // 透明度を1に
				}
			}
		}
	}


	// 選ばれているスプライトの拡大率を変化させる
	for (size_t i = 0; i < menuSprites_.size(); ++i) {
		if (menuSprites_[i]) {
			// 選択されているスプライトは拡大、そうでないスプライトは縮小
			float scale = 1.0f;
			if (static_cast<int>(i) == selectedIndex_) {
				scale = 256.0f;
			} else {
				scale = 200.0f;
			}
			// スプライトのTransform2Dコンポーネントを取得してスケールを設定
			if (auto* tr = menuSprites_[i]->GetComponent2D<Transform2D>()) {
				tr->SetScale(Vector2(scale*2.0f, scale));
			}
		}
	}

	// 選ばれているスプライトを右側になるように回転させる
	targetRotateOffset_ = -(2.0f * 3.14159265f / static_cast<float>(menuSprites_.size())) * selectedIndex_; // 選択されたスプライトを角度0.0の位置に
	rotateOffset_ = Application::MatsumotoUtility::SimpleEaseIn(rotateOffset_, targetRotateOffset_, 0.1f);

	// スプライトの位置と回転を更新
	for(int i = 0;i < static_cast<int>(menuSprites_.size()); ++i) {
		if (menuSprites_[i]) {
			// menuPosition_を中心として円状に配置し、中心を向かせる
			if (auto* tr = menuSprites_[i]->GetComponent2D<Transform2D>()) {
				float angle = static_cast<float>(i) * (360.0f / menuSprites_.size());
				float radius = 256.0f; // 中心からの距離
				float radian = (angle * 3.14159265f / 180.0f) + rotateOffset_;

				Vector2 offset(std::cos(radian) * radius, std::sin(radian) * radius);
				tr->SetTranslate(offset + menuAnchorPoint_);

				// 中心を向くように回転(中心方向のベクトルから角度を計算)
				float rotationAngle = std::atan2(-offset.y, -offset.x);
				tr->SetRotate(Vector3(0.0f, 0.0f, rotationAngle+3.14f));
			}
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
