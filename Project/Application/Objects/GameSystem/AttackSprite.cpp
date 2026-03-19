#include "AttackSprite.h"
#include <MatsumotoUtility.h>

void Application::AttackSprite::Initialize(std::function<KashipanEngine::Sprite* (const std::string&, const std::string&, KashipanEngine::DefaultSampler)> createSpriteFunc)
{
	velocity_ = Vector3(200.0f, -1.0f, 0.0f);
	animationTimer_ = 0.0f;

	playerSprite_ = createSpriteFunc("player", "attack_2P.png", KashipanEngine::DefaultSampler::LinearClamp);
	noiseSprite_ = createSpriteFunc("noise", "noise.png", KashipanEngine::DefaultSampler::LinearClamp);
	attackSprite_ = createSpriteFunc("hand", "hand.png", KashipanEngine::DefaultSampler::LinearClamp);
	
	MatsumotoUtility::SetTranslateToSprite(attackSprite_, Vector3(-1000.0f, -350.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(noiseSprite_, Vector3(5000.0f, -350.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(playerSprite_, Vector3(-1000.0f, -350.0f, 0.0f));

	MatsumotoUtility::ScaleSprite(noiseSprite_, 5.0f);
}

void Application::AttackSprite::Update()
{
	if (!attackSprite_) {
		return;
	}

	// スプライトの状態を戻す
	MatsumotoUtility::SimpleEaseSpriteFitToTexture(attackSprite_, 0.1f);
	
	MatsumotoUtility::SimpleEaseSpriteMove(playerSprite_, Vector3(-200.0f, -300.0f, 0.0f), 0.3f);

	// アニメーションのタイマーを更新
	if (animationTimer_ <= 0.0) {
		MatsumotoUtility::SimpleEaseSpriteColor(attackSprite_, Vector4(1.0f, 1.0f, 1.0f, 0.0f), 0.2f);
		MatsumotoUtility::SimpleEaseSpriteColor(playerSprite_, Vector4(1.0f, 1.0f, 1.0f, 0.0f), 0.2f);

		Vector3 noiseTargetPos = MatsumotoUtility::GetTranslateFromSprite(noiseSprite_) + velocity_;
		MatsumotoUtility::SetTranslateToSprite(noiseSprite_, noiseTargetPos);

		MatsumotoUtility::RotateSprite(noiseSprite_, Vector3(0.0f, 0.0f, 10.0f * KashipanEngine::GetDeltaTime()));

		MatsumotoUtility::SimpleEaseSpriteRotate(attackSprite_, Vector3(0.0f, 0.0f, 0.0f), 0.2f);
		MatsumotoUtility::SimpleEaseSpriteMove(attackSprite_, Vector3(-0.0f, -400.0f, 0.0f), 0.2f);
		
	}
	else {
		animationTimer_ -= KashipanEngine::GetDeltaTime();
		MatsumotoUtility::SimpleEaseSpriteColor(attackSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f), 0.5f);
		MatsumotoUtility::SimpleEaseSpriteColor(playerSprite_, Vector4(1.0f, 1.0f, 1.0f, 1.0f), 0.5f);

		MatsumotoUtility::SimpleEaseSpriteRotate(attackSprite_, Vector3(0.0f, 0.0f, 1.0f), 0.5f);
		MatsumotoUtility::SimpleEaseSpriteMove(attackSprite_, Vector3(-100.0f, -300.0f, 0.0f), 0.2f);

		Vector3 noiseTargetPos = MatsumotoUtility::GetTranslateFromSprite(attackSprite_) + Vector3(100.0f, 100.0f, 0.0f);
		MatsumotoUtility::SetTranslateToSprite(noiseSprite_, noiseTargetPos);
	}
}

void Application::AttackSprite::SetParent(KashipanEngine::Sprite* parent)
{
	if (attackSprite_ && parent) {
		MatsumotoUtility::ParentSpriteToSprite(attackSprite_, parent);
	}

	if(noiseSprite_ && parent) {
		MatsumotoUtility::ParentSpriteToSprite(noiseSprite_, parent);
	}

	if(playerSprite_ && parent) {
		MatsumotoUtility::ParentSpriteToSprite(playerSprite_, parent);
	}
}

void Application::AttackSprite::PlayAttackAnimation()
{
	animationTimer_ = 0.7f; // アニメーションの持続時間を設定
	MatsumotoUtility::SetRotationToSprite(attackSprite_, Vector3(0.0f, 0.0f, 1.14f * 0.5f));
	MatsumotoUtility::SetTranslateToSprite(attackSprite_, Vector3(-1000.0f, -250.0f, 0.0f));
	MatsumotoUtility::SetTranslateToSprite(playerSprite_, Vector3(-1000.0f, -250.0f, 0.0f));
}
