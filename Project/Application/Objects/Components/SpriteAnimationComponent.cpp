#include "SpriteAnimationComponent.h"

void Application::SpriteAnimationComponent::PlayRotateAnimetion(const Vector3& rotation, float animationTime, EaseType easeType) {
	if (isPlaying_) return;

	elapsedTime_ = 0.0f;
	animationDuration_ = animationTime;
	easeType_ = easeType;
	rotation_ = rotation;
	animationUpdateFunc_ = std::bind(&SpriteAnimationComponent::RotateAnimationUpdate, this);
	isPlaying_ = true;
}

void Application::SpriteAnimationComponent::ForcePlayRotateAnimetion(const Vector3& startRotation, const Vector3& targetRotation, float animationTime, EaseType easeType) {
	if(isPlaying_) {
		// 強制的に現在のアニメーションを終了させる
		elapsedTime_ = animationDuration_;
		Update(); // 現在のアニメーションを完了させるために1フレーム更新する
	}

	elapsedTime_ = 0.0f;
	animationDuration_ = animationTime;
	easeType_ = easeType;
	rotation_ = targetRotation;
	animationUpdateFunc_ = std::bind(&SpriteAnimationComponent::RotateAnimationUpdate, this);
	auto ctx = GetOwner2DContext();
	ctx->GetComponent<KashipanEngine::Transform2D>()->SetRotate(startRotation);
	isPlaying_ = true;
}

std::unique_ptr<KashipanEngine::IObjectComponent> Application::SpriteAnimationComponent::Clone() const {
	auto ptr = std::make_unique<SpriteAnimationComponent>();
	return ptr;
}

std::optional<bool> Application::SpriteAnimationComponent::Initialize() {
	isPlaying_ = false;
	elapsedTime_ = 0.0f;
	animationDuration_ = 0.0f;
    return true;
}

std::optional<bool> Application::SpriteAnimationComponent::Update() {
	if (!isPlaying_) return true;

	elapsedTime_ += KashipanEngine::GetDeltaTime();
	if (elapsedTime_ >= animationDuration_) {
		isPlaying_ = false;
	}

	if (animationUpdateFunc_) {
		animationUpdateFunc_();
	}

    return true;
}

void Application::SpriteAnimationComponent::RotateAnimationUpdate() {
	float t = std::clamp(elapsedTime_ / animationDuration_, 0.0f, 1.0f);
	auto ctx = GetOwner2DContext();
	Vector3 currentRotation = ctx->GetComponent<KashipanEngine::Transform2D>()->GetRotate();
	currentRotation = Eased(currentRotation, rotation_, t, easeType_);
	ctx->GetComponent<KashipanEngine::Transform2D>()->SetRotate(currentRotation);
}
