#include "ControllerViewer.h"
#include <MatsumotoUtility.h>

void Application::ControllerViewer::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&)> createSprite)
{
	createSpriteFunc_ = createSprite;
	controllerSprite_ = createSpriteFunc_("ControllerViewer", "controller.png");
	
	buttonSprites_.clear();
	buttonSprites_["a"] = createSpriteFunc_("ButtonSpriteA", "controller_a.png");
	buttonSprites_["b"] = createSpriteFunc_("ButtonSpriteB", "controller_b.png");
	buttonSprites_["x"] = createSpriteFunc_("ButtonSpriteX", "controller_x.png");
	buttonSprites_["y"] = createSpriteFunc_("ButtonSpriteY", "controller_y.png");
	buttonSprites_["move"] = createSpriteFunc_("ButtonSpriteMove", "controller_cross.png");
	buttonSprites_["lt"] = createSpriteFunc_("ButtonSpriteLT", "controller_lt.png");
	buttonSprites_["rt"] = createSpriteFunc_("ButtonSpriteRT", "controller_rt.png");

	// ボタンのスプライトは最初は非表示にする&コントローラーのスプライトを親にする
	for(auto& [spriteName, sprite] : buttonSprites_) {
		Application::MatsumotoUtility::SetColorToSprite(sprite, Vector4(1.0f, 1.0f, 1.0f, 0.0f));
		Application::MatsumotoUtility::ParentSpriteToSprite(sprite, controllerSprite_);
	}
}

void Application::ControllerViewer::Update(float delta)
{
	delta; // 現状はdeltaは使用しないが、将来的にアニメーションをつける際などに使用する可能性があるため引数として受け取る形にしている

	// すべてのボタンスプライトを非表示に近づける
	for(auto& [spriteName, sprite] : buttonSprites_) {
		Application::MatsumotoUtility::SimpleEaseSpriteColor(sprite, Vector4(1.0f, 1.0f, 1.0f, 0.0f), 0.1f);
	}

	// 入力があった場合可視化する
	for(auto& [inputName, checkFunc] : inputCheckFuncMap_) {
		if(checkFunc()) {
			if(buttonSprites_.find(inputName) != buttonSprites_.end()) {
				Application::MatsumotoUtility::SimpleEaseSpriteColor(buttonSprites_[inputName], Vector4(1.0f, 1.0f, 1.0f, 1.0f), 0.1f);
			}
		}
	}
}

void Application::ControllerViewer::SetInputCheckFunction(const std::string& inputName, std::function<bool()> checkFunc)
{
	inputCheckFuncMap_[inputName] = checkFunc;
}

void Application::ControllerViewer::SetTranslate(const Vector3& translate)
{
	Application::MatsumotoUtility::SetTranslateToSprite(controllerSprite_, translate);
}

void Application::ControllerViewer::SetScale(const Vector3& scale)
{
	Application::MatsumotoUtility::SetScaleToSprite(controllerSprite_, scale);
}

void Application::ControllerViewer::SetRotation(const Vector3& rotation)
{
	Application::MatsumotoUtility::SetRotationToSprite(controllerSprite_, rotation);
}

Vector3 Application::ControllerViewer::GetTranslate() const
{
	Vector3 translate = Application::MatsumotoUtility::GetTranslateFromSprite(controllerSprite_);
	return translate;
}

Vector3 Application::ControllerViewer::GetScale() const
{
	Vector3 scale = Application::MatsumotoUtility::GetScaleFromSprite(controllerSprite_);
	return scale;
}

Vector3 Application::ControllerViewer::GetRotation() const
{
	Vector3 rotation = Application::MatsumotoUtility::GetRotationFromSprite(controllerSprite_);
	return rotation;
}
