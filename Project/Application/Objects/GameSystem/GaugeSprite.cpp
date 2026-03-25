#include "GaugeSprite.h"
#include <MatsumotoUtility.h>

void Application::GaugeSprite::Initialize(
	std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc) {
	anchorSprite_ = createSpriteFunc("GaugeAnchor", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);
	
	gaugeSprite_ = createSpriteFunc("Gauge", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);
	gaugeAnimationSprite_ = createSpriteFunc("GaugeAnimation", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);
	gaugeFillSprite_ = createSpriteFunc("GaugeFill", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);
	gaugeFillAnimationSprite_ = createSpriteFunc("GaugeFillAnimation", "white1x1.png", KashipanEngine::DefaultSampler::LinearClamp);

	gaugeAnimationSprite_->SetPivotPoint(0.0f, 0.5f);
	gaugeFillSprite_->SetPivotPoint(0.0f, 0.5f);
	gaugeSprite_->SetPivotPoint(0.0f, 0.5f);

	gaugeFillAnimationSprite_->SetPivotPoint(0.0f, 0.5f);

	Application::MatsumotoUtility::ParentSpriteToSprite(gaugeFillSprite_, anchorSprite_);
	Application::MatsumotoUtility::ParentSpriteToSprite(gaugeAnimationSprite_, anchorSprite_);
	Application::MatsumotoUtility::ParentSpriteToSprite(gaugeSprite_, anchorSprite_);
	Application::MatsumotoUtility::ParentSpriteToSprite(gaugeFillAnimationSprite_, anchorSprite_);

	Application::MatsumotoUtility::SetColorToSprite(gaugeFillSprite_, Vector4(0.0f, 1.0f, 0.0f, 1.0f));
	Application::MatsumotoUtility::SetColorToSprite(gaugeAnimationSprite_, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
	Application::MatsumotoUtility::SetColorToSprite(gaugeSprite_, Vector4(0.0f, 0.0f, 0.0f, 1.0f));
	Application::MatsumotoUtility::SetColorToSprite(gaugeFillAnimationSprite_, Vector4(0.5f, 0.5f, 0.5f, 1.0f));


	size_ = Vector3(100.0f, 20.0f, 1.0f);
	SetSize(size_);
}

void Application::GaugeSprite::Update(float rate, float fillMaxRate)
{
	// 現在の最大値のサイズ
	Vector3 maxFillSize = size_;
	maxFillSize.x *=fillMaxRate;

	// 現在の最大減少量
	float maxDecrease = size_.x * (1.0f - fillMaxRate);

	// 現在のゲージサイズ (現在の最大値に対する割合)
	Vector3 currentSize = maxFillSize;
	currentSize.x *= rate;

	// ゲージ本体 (緑)
	if (gaugeFillSprite_) {
		Application::MatsumotoUtility::SetScaleToSprite(gaugeFillSprite_, currentSize);
	}

	// ゲージ減少のアニメーション (黄：本体のサイズにイージングで追従)
	if (gaugeAnimationSprite_) {
		Application::MatsumotoUtility::SimpleEaseSpriteScale(gaugeAnimationSprite_, currentSize, 0.2f);
	}

	// 最大値減少の表現 (グレー：用途に合わせて調整。現在は maxFillSize を負の方向へスケールなど)
	if (gaugeFillAnimationSprite_) {
		Application::MatsumotoUtility::SetScaleToSprite(gaugeFillAnimationSprite_, Vector3(-maxDecrease, maxFillSize.y, maxFillSize.z));
	}
}

void Application::GaugeSprite::SetSize(const Vector3& size)
{

	size_ = size;
	if (gaugeSprite_) {
		Application::MatsumotoUtility::SetScaleToSprite(gaugeSprite_, size);
	}
	if (gaugeFillSprite_) {
		Application::MatsumotoUtility::SetScaleToSprite(gaugeFillSprite_, size);
	}
	if (gaugeAnimationSprite_) {
		Application::MatsumotoUtility::SetScaleToSprite(gaugeAnimationSprite_, size);
	}
	if (gaugeFillAnimationSprite_) {
		// 最大値減少用スプライトの初期サイズは幅0にする
		Application::MatsumotoUtility::SetScaleToSprite(gaugeFillAnimationSprite_, Vector3(0.0f, size.y, size.z));
		Application::MatsumotoUtility::SetTranslateToSprite(gaugeFillAnimationSprite_, Vector3(size.x, 0.0f, 0.0f));
	}
}

void  Application::GaugeSprite::SetPosition(const Vector3& position)
{
	if (anchorSprite_) {
		Application::MatsumotoUtility::SetTranslateToSprite(anchorSprite_, position);
	}
}

void Application::GaugeSprite::SetRotation(const Vector3& rotation)
{
	if (anchorSprite_) {
		Application::MatsumotoUtility::SetRotationToSprite(anchorSprite_, rotation);
	}
}
void Application::GaugeSprite::SetParent(KashipanEngine::Sprite* parent)
{
	if (anchorSprite_ && parent) {
		Application::MatsumotoUtility::ParentSpriteToSprite(anchorSprite_, parent);
	}
}
