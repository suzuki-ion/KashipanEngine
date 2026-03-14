#include "MatsumotoUtility.h"

KashipanEngine::Sprite* Application::MatsumotoUtility::CreateSpriteObject(
	KashipanEngine::ScreenBuffer* screenBuffer2D,
	std::function<bool(std::unique_ptr<KashipanEngine::Object2DBase>obj)> AddObject, 
	const std::string& spriteName) {

	std::unique_ptr<KashipanEngine::Sprite> sprite = std::make_unique<KashipanEngine::Sprite>();
	KashipanEngine::Sprite* spritePtr = sprite.get();
	sprite->SetName(spriteName);
	sprite->SetUniqueBatchKey();
	sprite->SetPivotPoint(0.5f, 0.5f);

	assert(AddObject);
	assert(screenBuffer2D);

	if (screenBuffer2D) {
		sprite->AttachToRenderer(screenBuffer2D, "Object2D.DoubleSidedCulling.BlendNormal");
	}

	if(auto* mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		KashipanEngine::SamplerManager::SamplerHandle samplerHandle = KashipanEngine::SamplerManager::GetSampler(KashipanEngine::DefaultSampler::LinearWrap);
		mat->SetSampler(samplerHandle);
	}

	AddObject(std::move(sprite));
	return spritePtr;
}

void Application::MatsumotoUtility::SetTextureToSprite(KashipanEngine::Sprite* sprite, const std::string& textureName)
{
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		mat->SetTexture(KashipanEngine::TextureManager::GetTextureFromFileName(textureName));
	}
}

void Application::MatsumotoUtility::SetColorToSprite(KashipanEngine::Sprite* sprite, const Vector4& color)
{
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		mat->SetColor(color);
	}
}

Vector4 Application::MatsumotoUtility::GetColorFromSprite(KashipanEngine::Sprite* sprite)
{
	Vector4 result;
	if (!sprite) return result;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		result = mat->GetColor();
	}
	return  result;
}

void Application::MatsumotoUtility::SetTranslateToSprite(KashipanEngine::Sprite* sprite, const Vector3& translate)
{
	if (!sprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetTranslate(translate);
	}
}

Vector3 Application::MatsumotoUtility::GetTranslateFromSprite(KashipanEngine::Sprite* sprite)
{
	Vector3 result;
	if (!sprite) return result;

	if(auto* tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		result = tr->GetTranslate();
	}
	return result;
}

void Application::MatsumotoUtility::SetScaleToSprite(KashipanEngine::Sprite* sprite, const Vector3& scale)
{
	if (!sprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetScale(scale);
	}
}

Vector3 Application::MatsumotoUtility::GetScaleFromSprite(KashipanEngine::Sprite* sprite)
{
	Vector3 result;
	if (!sprite) return result;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		result = tr->GetScale();
	}
	return result;
}

void Application::MatsumotoUtility::SetRotationToSprite(KashipanEngine::Sprite* sprite, Vector3 rotation)
{
	if (!sprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		tr->SetRotate(rotation);
	}
}

Vector3 Application::MatsumotoUtility::GetRotationFromSprite(KashipanEngine::Sprite* sprite)
{
	Vector3 result;
	if (!sprite) return result;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		result = tr->GetRotate();
	}
	return result;
}

void Application::MatsumotoUtility::FitSpriteToTexture(KashipanEngine::Sprite* sprite)
{
	if (!sprite) return;
	auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>();
	if (!mat) return;
	Vector4 color = mat->GetColor();
	auto textureHandle = mat->GetTexture();
	if (textureHandle == KashipanEngine::TextureManager::kInvalidHandle) return;
	auto textureView = KashipanEngine::TextureManager::TextureView(textureHandle);
	Vector3 scale(static_cast<float>(textureView.GetWidth()), static_cast<float>(textureView.GetHeight()), 1.0f);
	SetScaleToSprite(sprite, scale);
}

void Application::MatsumotoUtility::ScaleSprite(KashipanEngine::Sprite* sprite, float n)
{
	if (!sprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		Vector3 scale = tr->GetScale();
		scale *= n;
		tr->SetScale(scale);
	}
}

void Application::MatsumotoUtility::ParentSpriteToSprite(KashipanEngine::Sprite* sprite, KashipanEngine::Sprite* rootSprite)
{
	if (!sprite || !rootSprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		if (auto *rootTr = rootSprite->GetComponent2D<KashipanEngine::Transform2D>()) {
			tr->SetParentTransform(rootTr);
		}
	}
}

void Application::MatsumotoUtility::HideSprite(KashipanEngine::Sprite* sprite)
{
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		Vector4 color = mat->GetColor();
		color.w = 0.0f;
		mat->SetColor(color);
	}
}

void Application::MatsumotoUtility::ShowSprite(KashipanEngine::Sprite* sprite)
{
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		Vector4 color = mat->GetColor();
		color.w = 1.0f;
		mat->SetColor(color);
	}
}

void Application::MatsumotoUtility::RotateSprite(KashipanEngine::Sprite* sprite, Vector3 deltaRotation)
{
	if (!sprite) return;
	if (auto *tr = sprite->GetComponent2D<KashipanEngine::Transform2D>()) {
		Vector3 rotation = tr->GetRotate();
		rotation += deltaRotation;
		tr->SetRotate(rotation);
	}
}

void Application::MatsumotoUtility::ChangeSpriteColorRGB(KashipanEngine::Sprite* sprite, const Vector3& rgb)
{
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		Vector4 color = mat->GetColor();
		color.x = rgb.x;
		color.y = rgb.y;
		color.z = rgb.z;
		mat->SetColor(color);
	}
}

Vector3 Application::MatsumotoUtility::GetTextureSizeFromSprite(KashipanEngine::Sprite* sprite)
{
	Vector3 result;
	if (!sprite) return result;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		auto textureHandle = mat->GetTexture();
		if (textureHandle == KashipanEngine::TextureManager::kInvalidHandle) return result;
		auto textureView = KashipanEngine::TextureManager::TextureView(textureHandle);
		result.x = static_cast<float>(textureView.GetWidth());
		result.y = static_cast<float>(textureView.GetHeight());
	}
	return result;
}

Vector2 Application::MatsumotoUtility::GetTextureUVFromSprite(KashipanEngine::Sprite* sprite) {
	Vector2 result;
	if (!sprite) return result;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		result.x = mat->GetUVTransform().translate.x;
		result.y = mat->GetUVTransform().translate.y;
	}
	return result;
}

void Application::MatsumotoUtility::SetTextureUVToSprite(KashipanEngine::Sprite* sprite, const Vector2& uv) {
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		auto uvTransform = mat->GetUVTransform();
		uvTransform.translate.x = uv.x;
		uvTransform.translate.y = uv.y;
		mat->SetUVTransform(uvTransform);
	}
}

void Application::MatsumotoUtility::MoveTextureUVToSprite(KashipanEngine::Sprite* sprite, const Vector2& deltaUv) {
	if (!sprite) return;
	if (auto *mat = sprite->GetComponent2D<KashipanEngine::Material2D>()) {
		auto uvTransform = mat->GetUVTransform();
		uvTransform.translate.x += deltaUv.x;
		uvTransform.translate.y += deltaUv.y;
		mat->SetUVTransform(uvTransform);
	}
}

float Application::MatsumotoUtility::SimpleEaseIn(float from, float to, float transitionSpeed)
{
	float value = from;
	value += (to - value) * transitionSpeed;
	if (fabsf(value - to) <= 0.01f) {
		return to;
	}
	return value;
}
