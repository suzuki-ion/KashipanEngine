#pragma once
#include <KashipanEngine.h>

namespace Application::MatsumotoUtility
{
	/// @brief スプライトオブジェクトの作成
	KashipanEngine::Sprite* CreateSpriteObject(
		KashipanEngine::ScreenBuffer* screenBuffer2D,
		std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase> obj)> AddObject,
		const std::string& spriteName);
	/// @brief スプライトにテクスチャを設定(拡張子を付けること)
	void SetTextureToSprite(KashipanEngine::Sprite* sprite, const std::string& textureName);
	/// @brief スプライトの色を設定
	void SetColorToSprite(KashipanEngine::Sprite* sprite, const Vector4& color);
	/// @brief スプライトの色を取得
	Vector4 GetColorFromSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトのtranslateを設定
	void SetTranslateToSprite(KashipanEngine::Sprite* sprite, const Vector3& translate);
	/// @brief スプライトのtranslateを取得
	Vector3 GetTranslateFromSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトのscaleを設定
	void SetScaleToSprite(KashipanEngine::Sprite* sprite, const Vector3& scale);
	/// @brief スプライトのscaleを取得
	Vector3 GetScaleFromSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトの回転を設定（単位はラジアン）
	void SetRotationToSprite(KashipanEngine::Sprite* sprite, Vector3 rotation);
	/// @brief スプライトの回転を取得（単位はラジアン）
	Vector3 GetRotationFromSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトのサイズをテクスチャのサイズに合わせる
	void FitSpriteToTexture(KashipanEngine::Sprite* sprite);
	/// @brief スプライトのスケールをn倍する
	void ScaleSprite(KashipanEngine::Sprite* sprite, float n);
	/// @brief スプライトをあるトランスフォームにペアレント
	void ParentSpriteToSprite(KashipanEngine::Sprite* sprite, KashipanEngine::Sprite* rootSprite);
	/// @brief スプライトを非表示にする(透明度を限界まで上げる)
	void HideSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトを表示する(透明度を限界まで下げる)
	void ShowSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトの回転をdeltaRotationだけ増加させる（単位はラジアン）
	void RotateSprite(KashipanEngine::Sprite* sprite, Vector3 deltaRotation);

	/// @brief 簡易イージング関数（イージングなしの線形補間に近い挙動）
	float SimpleEaseIn(float from, float to, float transitionSpeed);
}