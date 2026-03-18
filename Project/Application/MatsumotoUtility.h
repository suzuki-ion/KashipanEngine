#pragma once
#include <KashipanEngine.h>

namespace Application::MatsumotoUtility
{
	/// @brief スプライトオブジェクトの作成
	KashipanEngine::Sprite* CreateSpriteObject(
		KashipanEngine::ScreenBuffer* screenBuffer2D,
		std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase> obj)> AddObject,
        const std::string &spriteName, KashipanEngine::DefaultSampler defaultSampler);
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
	/// @brief スプライトの三原色だけを変更する（alphaは変更しない）
	void ChangeSpriteColorRGB(KashipanEngine::Sprite* sprite, const Vector3& rgb);
	/// @brief スプライトのテクスチャの大きさを取得
	Vector3 GetTextureSizeFromSprite(KashipanEngine::Sprite* sprite);

	/// @brief スプライトのテクスチャのUV座標を取得
	Vector2 GetTextureUVFromSprite(KashipanEngine::Sprite* sprite);
	/// @brief スプライトのテクスチャのUV座標を設定
	void SetTextureUVToSprite(KashipanEngine::Sprite* sprite, const Vector2& uv);
	/// @brief スプライトのテクスチャのUV座標を移動
	void MoveTextureUVToSprite(KashipanEngine::Sprite* sprite, const Vector2& deltaUv);

	/// @brief 簡易イージング関数を使ってスプライトをtargetPositionに移動させる
	void SimpleEaseSpriteMove(KashipanEngine::Sprite* sprite, const Vector3& targetPosition, float transitionSpeed);
	/// @brief 簡易イージング関数を使ってスプライトをtargetScaleに拡大縮小させる
	void SimpleEaseSpriteScale(KashipanEngine::Sprite* sprite, const Vector3& targetScale, float transitionSpeed);
	/// @brief 簡易イージング関数を使ってスプライトの色をtargetColorに変化させる
	void SimpleEaseSpriteColor(KashipanEngine::Sprite* sprite, const Vector4& targetColor, float transitionSpeed);
	/// @brief 簡易イージング関数を使ってスプライトをtargetRotationに回転させる
	void SimpleEaseSpriteRotate(KashipanEngine::Sprite* sprite, const Vector3& targetRotation, float transitionSpeed);
	/// @brief 簡易イージング関数を使ってスプライトのテクスチャの大きさに合わせてスケールを変化させる
	void SimpleEaseSpriteFitToTexture(KashipanEngine::Sprite* sprite, float transitionSpeed);

	/// @brief スケールの左右反転
	void FlipSpriteHorizontal(KashipanEngine::Sprite* sprite);

	/// @brief 簡易イージング関数（イージングなしの線形補間に近い挙動）
	float SimpleEaseIn(float from, float to, float transitionSpeed);

	/// @brief SEを再生する（SEのファイル名を渡す、拡張子も付けること）
	void PlaySE(const std::string& seName,float volume = 0.5f,float pich = 0.0f);
}